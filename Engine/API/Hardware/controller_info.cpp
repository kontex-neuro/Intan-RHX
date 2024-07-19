#include "controller_info.h"

#include <fmt/core.h>
#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <ostream>
#include <string>


using json = nlohmann::json;

XDAQInfo parse_info(const json &device_info)
{
    XDAQInfo info;

    info.FPGA_vender = device_info["FPGA Vender"];
    if (!info.FPGA_vender.contains("Opal Kelly")) {
        info.flash_memory = device_info["Flash Memory"];
    }
    info.serial = device_info["Serial Number"];
    const auto model = device_info["XDAQ Model"].template get<std::string>();

    if (model.contains("Core")) {
        info.model = XDAQModel::Core;
    } else if (model.contains("One")) {
        info.model = XDAQModel::One;
    } else {
        info.model = XDAQModel::Unknown;
    }

    if (model.contains("1")) {
        info.generation = 1;
    } else if (model.contains("2")) {
        info.generation = 2;
    }

    if (info.FPGA_vender.contains("Opal Kelly")) {
        info.expander = device_info["Expander"];
    }

    info.max_rhd_channels = device_info["RHD"];
    info.max_rhs_channels = device_info["RHS"];

    return info;
}


XDAQStatus parse_status(const json &device_status) {
    XDAQStatus status;
    
    status.version = device_status.at("Version");
    status.build = device_status.at("Build");
    status.mode = device_status.at("Mode");
    
    return status;
}