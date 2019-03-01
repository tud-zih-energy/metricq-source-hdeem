#include "hdeem_connection.hpp"
#include "hdeem_source.hpp"

#include <metricq/logger/nitro.hpp>
#include <metricq/types.hpp>

#include <hdeem_cxx.hpp>

#include <nitro/format/format.hpp>

using Log = metricq::logger::nitro::Log;

struct HDEEMMetric
{
    HDEEMMetric(std::string&& name, hdeem::sensor_id id) : name(std::move(name)), id(id)
    {
    }

    std::string name;
    hdeem::sensor_id id;
};

static std::vector<HDEEMMetric> initialize_metrics(hdeem::connection& connection,
                                                   const std::string& prefix,
                                                   const metricq::json& sensors);

void HDEEMConnection::run()
{
    // HDEEM is as reliable as machine learning.
    // so if things go wrong, we will loop a while here ¯\_(ツ)_/¯
    while (!stop_requested_)
    {
        try
        {
            Log::info() << "Connecting to " << bmc_host_;
            hdeem::connection connection(bmc_host_, bmc_user_, bmc_pw_);

            std::vector<HDEEMMetric> metrics =
                initialize_metrics(connection, metric_prefix_, sensors_);

            auto prev_stats = connection.get_stats_total();

            while (!stop_requested_)
            {
                // just wait some time, IMHO deadlines are pointless, as then a couple thousand
                // thread will try to wake up at the same time. So just let us go with the flow
                std::this_thread::sleep_for(interval_);

                // get current readings
                auto stats = connection.get_stats_total();

                for (auto& metric : metrics)
                {
                    auto duration = stats.time(metric.id) - prev_stats.time(metric.id);
                    auto energy = stats.energy(metric.id) - prev_stats.energy(metric.id);
                    double avg_power = energy / (duration.count() * 1e-9);

                    auto ts = stats.time(metric.id);

                    // TODO this crappy sanity check

                    source_.async_send(metric.name, ts, avg_power);
                }

                prev_stats = std::move(stats);
            }
        }
        catch (hdeem::error& e)
        {
            // We only catch HDEEM errors, the remainder might be actual errors and not HDEEM does
            // HDEEM things
            Log::error() << "Failure in connection to " << bmc_host_ << ": " << e.what();
            Log::info() << "Reseting connection to " << bmc_host_ << " in 5 seconds.";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

static std::vector<HDEEMMetric> initialize_metrics(hdeem::connection& connection,
                                                   const std::string& prefix,
                                                   const metricq::json& sensors)
{
    std::vector<HDEEMMetric> metrics;

    for (auto& sensor_name : sensors)
    {
        std::string sensor = sensor_name.get<std::string>();

        auto found = false;

        for (auto hdeem_id : connection.sensors())
        {
            if (connection.sensor_real_name(hdeem_id) == sensor)
            {
                std::string metric = nitro::format("{}.{}") % prefix % sensor;
                metrics.emplace_back(std::move(metric), hdeem_id);
                found = true;
            }
        }

        if (!found)
        {
            Log::warn() << "Couldn't find a sensor with the name: " << sensor;
        }
    }

    return metrics;
}
