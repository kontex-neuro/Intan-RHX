//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.3.1
//
//  Copyright (c) 2020-2023 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------
#include <fmt/format.h>
#include <xdaq/device_plugin.h>

#include <QApplication>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <vector>

#include "Engine/API/Hardware/controller_info.h"
#include "abstractrhxcontroller.h"
#include "boardselectdialog.h"
#include "commandparser.h"
#include "controllerinterface.h"
#include "controlwindow.h"
#include "datafilereader.h"
#include "rhxglobals.h"
#include "systemstate.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

auto get_plugins()
{
#ifdef __WIN32__
    std::vector<fs::path> search_path = {R"(.\plugins)"};
    constexpr auto extention = ".dll";
#elif __APPLE__
    std::vector<fs::path> search_path = {"/usr/local/lib/xdaq/plugins", "./plugins"};
    constexpr auto extention = ".dylib";
#elif __linux__
    std::vector<fs::path> search_path = {"/usr/local/lib/xdaq/plugins", "./plugins"};
    constexpr auto extention = ".so";
#endif
    auto hash = [](const fs::path &p) { return std::hash<std::string>()(p.string()); };
    std::unordered_set<fs::path, decltype(hash)> paths;
    for (const auto &p : search_path) {
        if (!fs::exists(p) || !fs::is_directory(p)) continue;
        for (const auto &entry : fs::directory_iterator(p)) {
            if (!fs::is_regular_file(entry) && !fs::is_symlink(entry)) continue;
            if (entry.path().extension() != extention) continue;
            paths.insert(fs::canonical(entry.path()));
        }
    }
    std::vector<xdaq::DevicePlugin> plugins;
    for (const auto &path : paths) {
        auto plugin = xdaq::load_device_plugin(path);
        if (plugin.has_value()) plugins.emplace_back(std::move(plugin.value()));
    }
    return std::move(plugins);
}

struct RHXAPP {
    // rhxController -> state (Q) -> controllerInterface (Q) -> parser (Q) -> controlWindow (Q: no
    // parent)
    std::unique_ptr<AbstractRHXController> rhxController;
    std::unique_ptr<SystemState> state;  // Keep the order of destruction
    // std::unique_ptr<ControlWindow> controlWindow;
    ControlWindow *controlWindow;
};

auto startSoftware(
    ControllerType controllerType, StimStepSize stimStepSize, DataFileReader *dataFileReader,
    AbstractRHXController *rhxController, QString defaultSettingsFile, bool useOpenCL, bool testMode
)
{
    bool is7310 = false;
    RHXAPP app{.rhxController = std::unique_ptr<AbstractRHXController>(rhxController)};
    XDAQInfo xdaqinfo;

    QSettings settings;

    app.state = std::make_unique<SystemState>(
        app.rhxController.get(),
        stimStepSize,
        controllerType == ControllerStimRecord ? 4 : 8,
        xdaqinfo.expander,
        testMode,
        dataFileReader,
        xdaqinfo.model == XDAQModel::One,
        (xdaqinfo.model == XDAQModel::Core ? 1 : 2)
    );

    // app.state->highDPIScaleFactor =
    //     main->devicePixelRatio();  // Use this to adjust graphics for high-DPI monitors.
    app.state->availableScreenResolution = QGuiApplication::primaryScreen()->geometry();
    auto controllerInterface = new ControllerInterface(
        app.state.get(),
        app.rhxController.get(),
        "",
        useOpenCL,
        dataFileReader,
        app.state.get(),
        is7310
    );
    app.state->setupGlobalSettingsLoadSave(controllerInterface);
    auto parser = new CommandParser(app.state.get(), controllerInterface, controllerInterface);
    app.controlWindow =
        new ControlWindow(app.state.get(), parser, controllerInterface, app.rhxController.get());
    // auto controlWindow = app.controlWindow.get();  // Keep weak pointer for easy access
    auto controlWindow = app.controlWindow;  // Keep weak pointer for easy access
    parser->controlWindow = controlWindow;

    QObject::connect(
        controlWindow,
        SIGNAL(sendExecuteCommand(QString)),
        parser,
        SLOT(executeCommandSlot(QString))
    );
    QObject::connect(
        controlWindow,
        SIGNAL(sendExecuteCommandWithParameter(QString, QString)),
        parser,
        SLOT(executeCommandWithParameterSlot(QString, QString))
    );
    QObject::connect(
        controlWindow, SIGNAL(sendGetCommand(QString)), parser, SLOT(getCommandSlot(QString))
    );
    QObject::connect(
        controlWindow,
        SIGNAL(sendSetCommand(QString, QString)),
        parser,
        SLOT(setCommandSlot(QString, QString))
    );

    QObject::connect(
        parser,
        SIGNAL(stimTriggerOn(QString)),
        controllerInterface,
        SLOT(manualStimTriggerOn(QString))
    );
    QObject::connect(
        parser,
        SIGNAL(stimTriggerOff(QString)),
        controllerInterface,
        SLOT(manualStimTriggerOff(QString))
    );
    QObject::connect(
        parser,
        SIGNAL(stimTriggerPulse(QString)),
        controllerInterface,
        SLOT(manualStimTriggerPulse(QString))
    );

    QObject::connect(parser, SIGNAL(updateGUIFromState()), controlWindow, SLOT(updateFromState()));
    QObject::connect(
        parser,
        SIGNAL(sendLiveNote(QString)),
        controllerInterface->saveThread(),
        SLOT(saveLiveNote(QString))
    );

    QObject::connect(
        controllerInterface, SIGNAL(TCPErrorMessage(QString)), parser, SLOT(TCPErrorSlot(QString))
    );

    if (dataFileReader) {
        QObject::connect(
            controlWindow,
            SIGNAL(setDataFileReaderSpeed(double)),
            dataFileReader,
            SLOT(setPlaybackSpeed(double))
        );
        QObject::connect(
            controlWindow, SIGNAL(setDataFileReaderLive(bool)), dataFileReader, SLOT(setLive(bool))
        );
        QObject::connect(controlWindow, SIGNAL(jumpToEnd()), dataFileReader, SLOT(jumpToEnd()));
        QObject::connect(controlWindow, SIGNAL(jumpToStart()), dataFileReader, SLOT(jumpToStart()));
        QObject::connect(
            controlWindow,
            SIGNAL(jumpToPosition(QString)),
            dataFileReader,
            SLOT(jumpToPosition(QString))
        );
        QObject::connect(
            controlWindow, SIGNAL(jumpRelative(double)), dataFileReader, SLOT(jumpRelative(double))
        );
        QObject::connect(
            controlWindow,
            SIGNAL(setStatusBarReadyPlayback()),
            dataFileReader,
            SLOT(setStatusBarReady())
        );
        QObject::connect(
            dataFileReader,
            SIGNAL(setStatusBar(QString)),
            controlWindow,
            SLOT(updateStatusBar(QString))
        );
        QObject::connect(
            dataFileReader,
            SIGNAL(setTimeLabel(QString)),
            controlWindow,
            SLOT(updateTimeLabel(QString))
        );
        QObject::connect(
            dataFileReader,
            SIGNAL(sendSetCommand(QString, QString)),
            parser,
            SLOT(setCommandSlot(QString, QString))
        );
    }

    QObject::connect(
        controllerInterface, SIGNAL(haveStopped()), controlWindow, SLOT(stopAndReportAnyErrors())
    );
    QObject::connect(
        controllerInterface,
        SIGNAL(setTimeLabel(QString)),
        controlWindow,
        SLOT(updateTimeLabel(QString))
    );
    QObject::connect(
        controllerInterface,
        SIGNAL(setTopStatusLabel(QString)),
        controlWindow,
        SLOT(updateTopStatusLabel(QString))
    );
    QObject::connect(
        controllerInterface,
        SIGNAL(setHardwareFifoStatus(double)),
        controlWindow,
        SLOT(updateHardwareFifoStatus(double))
    );
    QObject::connect(
        controllerInterface,
        SIGNAL(cpuLoadPercent(double)),
        controlWindow,
        SLOT(updateMainCpuLoad(double))
    );

    QObject::connect(
        controllerInterface->saveThread(),
        SIGNAL(setStatusBar(QString)),
        controlWindow,
        SLOT(updateStatusBar(QString))
    );
    QObject::connect(
        controllerInterface->saveThread(),
        SIGNAL(setTimeLabel(QString)),
        controlWindow,
        SLOT(updateTimeLabel(QString))
    );
    QObject::connect(
        controllerInterface->saveThread(),
        SIGNAL(sendSetCommand(QString, QString)),
        parser,
        SLOT(setCommandSlot(QString, QString))
    );
    QObject::connect(
        controllerInterface->saveThread(),
        SIGNAL(error(QString)),
        controlWindow,
        SLOT(queueErrorMessage(QString))
    );

    controlWindow->show();
    if (!defaultSettingsFile.isEmpty()) {
        if (controlWindow->loadSettingsFile(defaultSettingsFile)) {
            emit controlWindow->setStatusBar("Loaded default settings file " + defaultSettingsFile);
        } else {
            emit controlWindow->setStatusBar(
                "Error loading default settings file " + defaultSettingsFile
            );
        }
    }
    return std::move(app);
}

int main(int argc, char *argv[])
{
    auto plugins = get_plugins();
    std::vector<json> devices;
    std::vector<XDAQInfo> controller_infos;

    std::shared_ptr<xdaq::Device> opened_device;

    for (auto &plugin : plugins) {
        fmt::print("Plugin: {}\n", plugin.path);
        auto device = plugin.list_devices();
        fmt::print("Device: {}\n", device);
        auto device_json = json::parse(device);
        for (const auto &device : device_json) {
            auto dev = plugin.create(device.dump());
            auto info = read_xdaq_info(dev.get());
            info.plugin = plugin.path;
            info.device_config = device.dump();
            info.get_device = [&opened_device, &plugin](std::string device) {
                opened_device = plugin.create(device);
                opened_device->set_register_sync(0, 1 << 24, 1 << 24);
                return opened_device.get();
            };
            controller_infos.push_back(info);
        }
    }

    QApplication app(argc, argv);
    // Information used by QSettings to save basic settings across sessions.
    QCoreApplication::setOrganizationName(OrganizationName);
    QCoreApplication::setOrganizationDomain(OrganizationDomain);
    QCoreApplication::setApplicationName(ApplicationName);
    // Globally disable unused Context Help buttons from windows/dialogs
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

#ifdef __APPLE__
    app.setStyle(QStyleFactory::create("Fusion"));
#endif

    BoardSelectDialog boardSelectDialog(nullptr, controller_infos);
    // print size of boardSelectDialog
    std::vector<RHXAPP> apps;
    boardSelectDialog.show();
    QObject::connect(
        &boardSelectDialog,
        &BoardSelectDialog::launch,
        [&apps, &boardSelectDialog](
            AbstractRHXController *controller,
            StimStepSize step_size,
            DataFileReader *data_file,
            bool OpenCL,
            bool test_mode
        ) {
            apps.push_back(std::move(startSoftware(
                controller->getType(), step_size, data_file, controller, "", OpenCL, test_mode
            )));
            boardSelectDialog.accept();
        }
    );

    return app.exec();
}
