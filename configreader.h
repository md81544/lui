#pragma once

#include <flat_map>
#include <string>
#include <string_view>
#include <variant>

// NOTE! This supports a VERY simplified YAML sytax.
// All that it supports is key : value pairs,
// and SINGLE-LEVEL blocks, which can be indented by any number of
// spaces.
// For example a config file might look like:
//
// foo : 23
// bar:
//   baz: true
//
// The caller can call readDouble("foo") to get 23
// Or readBool("bar/baz") to get true.
//
// Note: if the wrong type is attempted (e.g. readBool on a numeric value)
// then an assertion will occur in debug mode, and the default value will
// be returned in release mode.

namespace mgo {

using CfgValueType = std::variant<double, bool, std::string>;

class IConfigReader {
public:
    virtual double readDouble(std::string_view key, double defaultValue) const = 0;
    virtual bool readBool(std::string_view key, bool defaultValue) const = 0;
    virtual std::string readString(std::string_view key, const std::string& defaultValue) const = 0;
    virtual ~IConfigReader() = default;

private:
    virtual CfgValueType read(std::string_view key, const CfgValueType& defaultValue) const = 0;
};

// Mock config reader always returns the defaultValue
class MockConfigReader final : public IConfigReader {
public:
    virtual double readDouble(std::string_view, double defaultValue) const override
    {
        return defaultValue;
    }
    virtual bool readBool(std::string_view, bool defaultValue) const override
    {
        return defaultValue;
    }
    virtual std::string readString(std::string_view, const std::string& defaultValue) const override
    {
        return defaultValue;
    }

private:
    // This is unused
    virtual CfgValueType read(std::string_view, const CfgValueType& defaultValue) const override
    {
        return defaultValue;
    }
};

class ConfigReader final : public IConfigReader {
public:
    ConfigReader(std::string_view sConfigFileName);
    ~ConfigReader() { }
    // Cannot templatize virtual functions, hence the following "specializations"
    // which suffice for our needs. If ConfigReader did not inherit from IConfigReader we
    // could templatize, but then we'd lose the ability to mock it out. This approach
    // avoids the caller needing to use std::get on a variant result. See notes above -
    // these functions will assert(false) if the wrong type is attempted to be read in
    // debug mode, and the default will be silently returned in release mode.
    virtual double readDouble(std::string_view key, double defaultValue) const override;
    virtual bool readBool(std::string_view key, bool defaultValue) const override;
    virtual std::string
    readString(std::string_view key, const std::string& defaultValue) const override;

private:
    virtual CfgValueType
    read(std::string_view key, const CfgValueType& defaultValue) const override;

private:
    std::flat_map<std::string, CfgValueType> m_map;
};

} // namespace mgo
