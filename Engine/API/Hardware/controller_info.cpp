#include "controller_info.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <string>


using json = nlohmann::json;

XDAQInfo parse_info(const json &device_info)
{
    XDAQInfo info;

    info.FPGA_vender =
        device_info.contains("FPGA Vender") ? device_info["FPGA Vender"].get<std::string>() : "";
    info.serial = device_info["Serial Number"];
    const auto model = device_info["XDAQ Model"].get<std::string>();

    if (model.contains("Core")) {
        info.model = XDAQModel::Core;
    } else if (model.contains("One")) {
        info.model = XDAQModel::One;
    } else if (model.contains("AIO")) {
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

    info.max_rhd_channels = device_info.contains("RHD") ? device_info["RHD"].get<int>() : 0;
    info.max_rhs_channels = device_info.contains("RHS") ? device_info["RHS"].get<int>() : 0;

    return info;
}


XDAQStatus parse_status(const json &device_status)
{
    XDAQStatus status;

    status.version = device_status.at("Version");
    status.build = device_status.at("Build");
    status.mode = device_status.at("Mode");

    return status;
}