#include "hdeem_source.hpp"

#include <metricq/json.hpp>
#include <metricq/logger/nitro.hpp>
#include <metricq/types.hpp>

#include <nitro/format/format.hpp>

#include <chrono>

#include <cmath>

using Log = metricq::logger::nitro::Log;

HDEEMSource::HDEEMSource(const std::string& manager_host, const std::string& token)
: metricq::Source(token), signals_(io_service, SIGINT, SIGTERM)
{
    Log::debug() << "HDEEMSource::HDEEMSource() called";

    // Register signal handlers so that the daemon may be shut down.
    signals_.async_wait([this](auto, auto signal) {
        if (!signal)
        {
            return;
        }
        Log::info() << "Caught signal " << signal << ". Shutdown.";

        stop();
    });

    connect(manager_host);
}

HDEEMSource::~HDEEMSource()
{
}

void HDEEMSource::on_source_config(const metricq::json& config)
{
    Log::debug() << "HDEEMSource::on_source_config() called";

    std::string metric_pattern = config.at("metric");
    std::string bmc_pattern = config.at("bmc").at("hostname");

    std::string bmc_user = config.at("bmc").at("user");
    std::string bmc_pw = config.at("bmc").at("pw");

    for (auto& nodes : config.at("nodes"))
    {
        int begin = nodes.at("begin");
        int end = nodes.at("end");

        for (int i = begin; i <= end; i++)
        {
            std::string metric_prefix = nitro::format(metric_pattern) % i;
            std::string bmc_hostname = nitro::format(bmc_pattern) % i;

            connections_.emplace_back(std::make_unique<HDEEMConnection>(
                *this, metric_prefix, bmc_hostname, bmc_user, bmc_pw, config.at("sensors"),
                std::chrono::milliseconds(int(1000 / config.at("rate").get<double>()))));

            for (std::string sensor : config.at("sensors"))
            {
                auto metric_name = nitro::format("{}.{}.power") % metric_prefix % sensor;
                Log::info() << metric_name;

                auto& metric = (*this)[metric_name];
                metric.metadata.unit("W");
            }
        }
    }
}

void HDEEMSource::on_source_ready()
{
    Log::debug() << "HDEEMSource::on_source_ready() called";
    running_ = true;
    for (auto& connection : connections_)
    {
        connection->start();
    }
}

void HDEEMSource::on_error(const std::string& message)
{
    Log::debug() << "HDEEMSource::on_error() called";
    Log::error() << "Shit hits the fan: " << message;
    signals_.cancel();
    running_ = false;
    for (auto& connection : connections_)
    {
        connection->stop();
    }
}

void HDEEMSource::on_closed()
{
    Log::debug() << "HDEEMSource::on_closed() called";
    signals_.cancel();
    running_ = false;
    for (auto& connection : connections_)
    {
        connection->stop();
    }
}

void HDEEMSource::async_write(const std::string& metric, metricq::TimePoint timestamp, double value)
{

    // post this as a new task, so we can cross thread boundaries
    io_service.post([metric, timestamp, value, this]() {
        if (!this->running_)
        {
            return;
        }

        // trigger the write to the metricq::Metric from the processing thread
        (*this)[metric].send({ timestamp, value });
    });
}
