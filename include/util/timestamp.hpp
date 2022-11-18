#pragma once

#include <chrono>
#include <common.hpp>
#include <message.hpp>
#include <params.hpp>

namespace ligero::aya {

/************************************************************
 * Struct where we store global timestamp configurations.
 ************************************************************/
struct timestamp_config {
    const bool& enabled() const noexcept { return enabled_; }
    void enable()  noexcept { enabled_ = true; }
    void disable() noexcept { enabled_ = false; }

protected:
    bool enabled_ = true;  /**< whether enable sending timestamps */
};


/************************************************************
 * Define a global accessable config object.
 *
 * Example usage: timestamp_config_t::instance().enable()
 ************************************************************/
using timestamp_config_t = singleton<timestamp_config>;


/************************************************************
 * Return unix-like seconds since UTC 1970-1-1
 *
 * @return  Seconds count since UTC 1970-1-1
 ************************************************************/
template <typename Resolution = std::chrono::milliseconds>
size_t tick_since_epoch() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<Resolution>(now.time_since_epoch()).count();
}


/************************************************************
 * Send current time as a timestamp to remote server
 *
 * Note: configurations in `timestamp_config_t` will affect
 *       behaviour of this function
 *
 * @param t     Generalized transport
 * @param name  Name of the event
 ************************************************************/
template <typename Transport>
void send_timestamp(Transport&& t, std::string name) {
    if (timestamp_config_t::instance().enabled()) {
        t.send(header_t::DIAG_STOPWATCH_SPLIT, name);
        t.template receive<header_t>(params::generic_receive_timeout);
    }
}

} // namespace ligero::aya

