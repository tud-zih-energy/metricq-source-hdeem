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

        send_possible_ = false;

        for (auto& connection : connections_)
        {
            connection->stop();
        }

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
                Log::debug() << "New metric: " << metric_name;

                auto& metric = (*this)[metric_name];
                metric.metadata.unit("W");
                metric.chunk_size(config.at("chunk_size").get<int>());
            }
        }
    }
}

void HDEEMSource::on_source_ready()
{
    Log::debug() << "HDEEMSource::on_source_ready() called";
    send_possible_ = true;
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

    for (auto& connection : connections_)
    {
        connection->stop();
    }

    send_possible_ = false;
}

void HDEEMSource::on_closed()
{
    Log::debug() << "HDEEMSource::on_closed() called";
    signals_.cancel();
    send_possible_ = false;
    for (auto& connection : connections_)
    {
        connection->stop();
    }
}

void HDEEMSource::async_send(const std::vector<MetricTimeValues>& mtv)
{
    // post this as a new task, so we can cross thread boundaries
    io_service.post([mtv, this]() {
        if (!this->send_possible_)
        {
            return;
        }

        // trigger the write to the metricq::Metric from the processing thread
        for (const auto& entry : mtv)
        {
            (*this)[entry.metric].send({ entry.timestamp, entry.value });
        }
    });
}
