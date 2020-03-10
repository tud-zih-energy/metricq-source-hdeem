#pragma once

#include "hdeem_connection.hpp"

#include <metricq/source.hpp>
#include <metricq/timer.hpp>

#include <asio/signal_set.hpp>

#include <memory>
#include <vector>

class HDEEMSource : public metricq::Source
{
public:
    HDEEMSource(const std::string& manager_host, const std::string& token);
    ~HDEEMSource();

    void on_error(const std::string& message) override;
    void on_closed() override;

    void async_send(const HDEEMConnection& p, const std::string& metric,
                    metricq::TimePoint timestamp, double value);

private:
    void on_source_config(const metricq::json& config) override;
    void on_source_ready() override;

    void start_measurements();

    asio::signal_set signals_;

    std::vector<std::shared_ptr<HDEEMConnection>> connections_;
    bool send_possible_ = false;
};
