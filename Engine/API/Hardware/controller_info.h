#pragma once

#include <xdaq/device_manager.h>

#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <string>


using json = nlohmann::json;

enum class XDAQModel { Unknown = 0, Core = 1, One = 3 };

struct XDAQInfo {
    std::string plugin;
    std::string id;
    std::string FPGA_vender;
    std::string flash_memory;
    XDAQModel model = XDAQModel::Unknown;
    int generation = 1;
    std::string serial = "N/A";

    int max_rhd_channels = 0;
    int max_rhs_channels = 0;
    bool expander = false;
    std::string device_config = "N/A";
    std::function<xdaq::DeviceManager::OwnedDevice(const std::string &)> get_device = nullptr;
};

struct XDAQStatus {
    std::string version;
    std::string build;
    std::string mode;
};

XDAQInfo parse_info(const json &device_info);
XDAQStatus parse_status(const json &device_status);