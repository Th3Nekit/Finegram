#ifndef TGCALLS_CUSTOM_PARAMETERS_H
#define TGCALLS_CUSTOM_PARAMETERS_H

#include <map>
#include <string>

#include "third-party/json11.hpp"

namespace tgcalls {

inline bool getCustomParameterBool(std::map<std::string, json11::Json> const &parameters, std::string const &name) {
    const auto value = parameters.find(name);
    if (value != parameters.end() && value->second.is_bool() && value->second.bool_value()) {
        return true;
    } else {
        return false;
    }
}

inline bool getCustomParameterBoolDefaultTrue(std::map<std::string, json11::Json> const &parameters, std::string const &name) {
    const auto value = parameters.find(name);
    if (value != parameters.end() && value->second.is_bool()) {
        return value->second.bool_value();
    }
    return true;
}

inline int getCustomParameterInt(std::map<std::string, json11::Json> const &parameters, std::string const &name) {
    const auto value = parameters.find(name);
    if (value != parameters.end() && value->second.is_number()) {
        return value->second.int_value();
    }
    return 0;
}

}

#endif
