#pragma once

#include <metricq/json.hpp>
#include <metricq/logger/nitro.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class HDEEMSource;

class HDEEMConnection : public std::enable_shared_from_this<HDEEMConnection>
{
public:
    HDEEMConnection(HDEEMSource& source, const std::string& metric_prefix,
                    const std::string& bmc_host, const std::string& bmc_user,
                    const std::string& bmc_pw, const metricq::json& sensors,
                    std::chrono::milliseconds interval)
    : source_(source), metric_prefix_(metric_prefix), bmc_host_(bmc_host), bmc_user_(bmc_user),
      bmc_pw_(bmc_pw), sensors_(sensors), interval_(interval)
    {
    }

    ~HDEEMConnection()
    {
        if (running_)
        {
            stop();
        }

        thread_.join();
    }

public:
    void start()
    {
        if (running_)
        {
            metricq::logger::nitro::Log::warn()
                << "Thread is already running, but requested to start.";
            return;
        }
        running_ = true;
        thread_ = std::thread(&HDEEMConnection::run, this);
    }

    void stop()
    {
        if (!running_)
        {
            metricq::logger::nitro::Log::warn() << "Thread is not running, but requested to stop.";
            return;
        }

        stop_requested_ = true;
        running_ = false;
    }

private:
    void run();

private:
    std::thread thread_;
    HDEEMSource& source_;
    std::string metric_prefix_;
    std::string bmc_host_;
    std::string bmc_user_;
    std::string bmc_pw_;
    metricq::json sensors_;
    std::chrono::milliseconds interval_;
    bool running_ = false;
    std::atomic<bool> stop_requested_ = false;
};
