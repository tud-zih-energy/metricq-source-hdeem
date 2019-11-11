#include "hdeem_source.hpp"

#include <metricq/logger/nitro.hpp>

#include <nitro/broken_options/parser.hpp>

#include <iostream>
#include <string>

using Log = metricq::logger::nitro::Log;

int main(int argc, char* argv[])
{
    metricq::logger::nitro::set_severity(nitro::log::severity_level::info);

    nitro::broken_options::parser parser;
    parser.option("server", "The metricq management server to connect to.")
        .default_value("amqp://localhost")
        .short_name("s");
    parser.option("token", "The token used for source authentication against the metricq manager.")
        .default_value("source-hdeeem");
    parser.toggle("verbose").short_name("v");
    parser.toggle("trace").short_name("t");
    parser.toggle("quiet").short_name("q");
    parser.toggle("help").short_name("h");

    try
    {
        auto options = parser.parse(argc, argv);

        if (options.given("help"))
        {
            parser.usage();
            return 0;
        }

        if (options.given("trace"))
        {
            metricq::logger::nitro::set_severity(nitro::log::severity_level::trace);
        }
        else if (options.given("verbose"))
        {
            metricq::logger::nitro::set_severity(nitro::log::severity_level::debug);
        }
        else if (options.given("quiet"))
        {
            metricq::logger::nitro::set_severity(nitro::log::severity_level::warn);
        }

        metricq::logger::nitro::initialize();

        HDEEMSource source(options.get("server"), options.get("token"));
        Log::info() << "starting main loop.";
        source.main_loop();
        Log::info() << "exiting main loop.";

        return 0;
    }
    catch (nitro::broken_options::parsing_error& e)
    {
        std::cerr << e.what() << '\n';
        parser.usage();
        return 1;
    }
    catch (std::exception& e)
    {
        Log::error() << "Unhandled exception: " << e.what();
        return 2;
    }
}
