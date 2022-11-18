#pragma once

#include <string>
#include <string_view>
#include <tuple>

namespace ligero::aya {

constexpr bool split_keep_delimiter = true;
constexpr bool split_discard_delimiter = false;

/************************************************************
 * Split a string at first occurance of delimiter.
 *
 * Example:
 *     split_at("foo bar")          -> ("foo", "bar")
 *     split_at("fooABCbar", "ABC") -> ("foo", "bar")
 *     split_at("foobar")           -> ("foobar", "")
 *     split_at("foobar", "/")      -> ("foobar", "")
 *     split_at("foo/bar", "/", split_keep_delimiter) -> ("foo", "/bar")
 *
 * @param str             Target string
 * @param delimiter       Delimiter
 * @param keep_delimiter  Whether to keep the delimiter
 * @return  A pair of splitted string
 ************************************************************/
std::tuple<std::string, std::string>
split_at(const std::string& str, const std::string& delimiter = " ", bool keep_delimiter = split_discard_delimiter) {
    auto it = str.find(delimiter);

    if (it != std::string::npos) {
        return keep_delimiter
            ? std::make_tuple(str.substr(0, it), str.substr(it))
            : std::make_tuple(str.substr(0, it), str.substr(it + delimiter.size()));
    }
    else {
        return { str, "" };
    }
}

} // namespace ligero::aya

