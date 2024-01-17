#pragma once
#include <xdaq/device.h>

#include <functional>
#include <string>

enum class XDAQModel { Unknown = 0, Core = 1, One = 3 };

struct XDAQInfo {
    std::string plugin;
    std::string id;
    std::string serial;
    XDAQModel model = XDAQModel::Unknown;

    int max_rhd_channels = 0;
    int max_rhs_channels = 0;
    bool expander = false;
    std::string device_config;
    std::function<xdaq::Device *(std::string)> get_device = nullptr;
};

XDAQInfo read_xdaq_info(xdaq::Device *dev);