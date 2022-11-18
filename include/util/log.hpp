#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>

#define TRACE BOOST_LOG_TRIVIAL(trace)
#define DEBUG BOOST_LOG_TRIVIAL(debug)
#define INFO BOOST_LOG_TRIVIAL(info)
#define WARNING BOOST_LOG_TRIVIAL(warning)
#define ERROR BOOST_LOG_TRIVIAL(error)
#define FATAL BOOST_LOG_TRIVIAL(fatal)

namespace logging = boost::log;

enum class log_level : unsigned char {
    disabled,
    debug_only,
    info_only,
    full,
};

void enable_logging();
void disable_logging();
void set_logging_level(log_level level);
