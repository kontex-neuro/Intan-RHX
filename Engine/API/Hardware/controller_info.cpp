#include "controller_info.h"

#include <fmt/format.h>

#include <chrono>
#include <thread>

bool wait_xdaq(xdaq::Device *dev, int retry)
{
    using Clock = std::chrono::high_resolution_clock;
    for (; retry >= 0; --retry) {
        const auto start = Clock::now();
        while (true) {
            if ((*dev->get_register_sync(0x22) & 0x4) == 0) return true;
            if ((Clock::now() - start) > std::chrono::seconds(3)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (retry == 0) return false;
        dev->trigger(0x48, 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

XDAQInfo read_xdaq_info(xdaq::Device *dev)
{
    if (!wait_xdaq(dev, 1)) {
        throw std::runtime_error("XDAQ is not ready, please restart the device and try again");
    }
    XDAQInfo info;
    info.serial = fmt::format("{:08x}", *dev->get_register_sync(0x32));
    const auto hdmi = *dev->get_register_sync(0x31);
    switch ((hdmi & 0xFF)) {
    case 0:
        if (((hdmi & 0xFF00) >> 8) == 1)
            info.model = XDAQModel::Core;
        else if (((hdmi & 0xFF00) >> 8) == 3)
            info.model = XDAQModel::One;
        else
            info.model = XDAQModel::Unknown;
        break;
    case 1: info.model = XDAQModel::Core; break;
    case 2: info.model = XDAQModel::One; break;
    default: info.model = XDAQModel::Unknown;
    }
    info.expander = (*dev->get_register_sync(0x35)) != 0;
    info.max_rhs_channels = ((hdmi >> 16) & 0xFF) * 16;
    info.max_rhd_channels = ((hdmi >> 24) & 0xFF) * 32;
    if (info.model == XDAQModel::Core) {
        info.max_rhd_channels /= 2;
    }
    return info;
}