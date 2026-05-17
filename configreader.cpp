#include "configreader.h"
#include "ascii.h"
#include "utils.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace {

std::optional<double> isNumeric(std::string_view s)
{
    if (s.empty()) {
        return std::nullopt;
    }
    std::string stripped;
    stripped.reserve(s.size());
    std::copy_if(s.begin(), s.end(), std::back_inserter(stripped), [](char c) { return c != ','; });
    const char* first = stripped.data();
    const char* last = stripped.data() + stripped.size();
    double val;
    auto [p, e] = std::from_chars(first, last, val);
    if (e == std::errc {} && p == last) {
        return val;
    }
    return std::nullopt;
}

} // anonymous namespace

namespace mgo {

ConfigReader::ConfigReader(std::string_view cfgfile)
{
    // Open the config file
    std::ifstream ifs;
    ifs.open(cfgfile.data(), std::ios::in);
    if (!ifs) {
        throw std::runtime_error(std::format("Could not open {}", cfgfile));
    }
    std::string line;
    size_t n;
    std::string outerKey;
    while (getline(ifs, line)) {
        bool indented = false;
        if (line.size() > 0 && ascii::isspace(line[0])) {
            indented = true;
        }
        utils::trim(line);
        if (!line.empty() && line[0] != '#') {
            n = line.find(":");
            if (n > 0) {
                std::string key = line.substr(0, n);
                utils::trim(key);
                std::transform(key.begin(), key.end(), key.begin(), ascii::toupper);
                if (!indented) {
                    outerKey = key;
                }
                std::string value = line.substr(n + 1);
                utils::trim(value);
                if (indented) {
                    key = outerKey + "/" + key;
                }
                if (!value.empty()) {
                    if (value == "true") {
                        m_map[key] = true;
                    } else if (value == "false") {
                        m_map[key] = false;
                    } else if (auto val = isNumeric(value)) {
                        m_map[key] = *val;
                    } else {
                        m_map[key] = value;
                    }
                }
            }
        }
    }
    ifs.close();
}

CfgValueType ConfigReader::read(std::string_view key, const CfgValueType& defaultValue) const
{
    std::string k(key);
    std::transform(k.begin(), k.end(), k.begin(), ascii::toupper);
    auto it = m_map.find(k.data());
    if (it != m_map.end()) {
        return it->second;
    }
    return defaultValue;
}

double ConfigReader::readDouble(std::string_view key, double defaultValue) const
{
    try {
        auto val = std::get<double>(read(key, defaultValue));
        return val;
    } catch (std::bad_variant_access const&) {
        assert(false);
        return defaultValue;
    }
}

bool ConfigReader::readBool(std::string_view key, bool defaultValue) const
{
    try {
        auto val = std::get<bool>(read(key, defaultValue));
        return val;
    } catch (std::bad_variant_access const&) {
        assert(false);
        return defaultValue;
    }
}

std::string ConfigReader::readString(std::string_view key, const std::string& defaultValue) const
{
    try {
        auto val = std::get<std::string>(read(key, defaultValue));
        return val;
    } catch (std::bad_variant_access const&) {
        assert(false);
        return defaultValue;
    }
}

} // mgo