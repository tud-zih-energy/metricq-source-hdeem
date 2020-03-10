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

    // for reconfige, we first stop all connection threads
    for (auto& connection : connections_)
    {
        connection->stop();
    }
    connections_.clear();

    // all threads are joined in the destructor, so we can now clear other state
    clear_metrics();

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
                std::string metric_name;
                if (sensor == "blade")
                {
                    metric_name = nitro::format("{}.power") % metric_prefix;
                }
                else
                {
                    metric_name = nitro::format("{}.{}.power") % metric_prefix % sensor;
                }
                Log::debug() << "New metric: " << metric_name;

                auto& metric = (*this)[metric_name];
                metric.metadata.unit("W");
                metric.metadata.rate(config.at("rate").get<double>());
                metric.metadata.scope(metricq::Metadata::Scope::last);
                metric.metadata["bmc"] = bmc_hostname;
                metric.metadata.description(
                    nitro::format("HDEEM power measurements from '{}' of sensor '{}'") %
                    bmc_hostname % sensor);
                metric.chunk_size(config.at("chunk_size").get<int>());
            }
        }
    }

    // in case of reconfigure, start_measurement here, otherwise, on_source_ready will do it.
    if (send_possible_)
    {
        start_measurements();
    }
}

void HDEEMSource::on_source_ready()
{
    Log::debug() << "HDEEMSource::on_source_ready() called";
    start_measurements();
}

void HDEEMSource::start_measurements()
{
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

void HDEEMSource::async_send(const HDEEMConnection& conn, const std::string& metric,
                             metricq::TimePoint timestamp, double value)
{
    // post this as a new task, so we can cross thread boundaries
    io_service.post([conn = conn.weak_from_this(), metric, timestamp, value, this]() {
        if (!this->send_possible_)
        {
            return;
        }

        // this thread is executed in the main thread, which is the only one calling the
        // destructor so this check is fine
        if (conn.expired())
        {
            return;
        }

        // trigger the write to the metricq::Metric from the processing thread
        (*this)[metric].send({ timestamp, value });
    });
}
