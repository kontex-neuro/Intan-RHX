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
#include <fmt/core.h>
#include <fmt/format.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qglobal.h>
#include <qsettings.h>
#include <xdaq/device_manager.h>

#include <QApplication>
#include <memory>
#include <nlohmann/json.hpp>
#include <ostream>
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
    AbstractRHXController *rhxController, QString defaultSettingsFile, bool useOpenCL,
    bool testMode, XDAQInfo info
)
{
    bool is7310 = false;
    RHXAPP app{.rhxController = std::unique_ptr<AbstractRHXController>(rhxController)};
    QSettings settings;

    app.state = std::make_unique<SystemState>(
        app.rhxController.get(),
        stimStepSize,
        controllerType == ControllerStimRecord ? 4 : 8,
        info.expander,
        testMode,
        dataFileReader,
        info.model == XDAQModel::One,
        (info.model == XDAQModel::Core ? 1 : 2)
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
    QApplication app(argc, argv);
    // Information used by QSettings to save basic settings across sessions.
    QCoreApplication::setOrganizationName(OrganizationName);
    QCoreApplication::setOrganizationDomain(OrganizationDomain);
    QCoreApplication::setApplicationName(ApplicationName);
    // Globally disable unused Context Help buttons from windows/dialogs
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
    QSettings settings;
    // fmt::println("settings = ", settings.fileName().toStdString());

#ifdef __APPLE__
    app.setStyle(QStyleFactory::create("Fusion"));
#endif

    BoardSelectDialog boardSelectDialog(nullptr);
    // std::vector<RHXAPP> apps;
    RHXAPP rhx_app;
    boardSelectDialog.show();
    QObject::connect(
        &boardSelectDialog,
        &BoardSelectDialog::launch,
        [&rhx_app, &boardSelectDialog](
            AbstractRHXController *controller,
            StimStepSize step_size,
            DataFileReader *data_file,
            bool OpenCL,
            bool test_mode,
            XDAQInfo info
        ) {
            auto splash = new QSplashScreen(QPixmap(":images/RHX_splash.png"));
            auto splashMessage = "Copyright " + CopyrightSymbol + " " + ApplicationCopyrightYear +
                                 " Intan Technologies.  RHX version " + SoftwareVersion +
                                 ".  Opening XDAQ ...";
            auto splashMessageAlign = Qt::AlignCenter | Qt::AlignBottom;
            auto splashMessageColor = Qt::white;

            boardSelectDialog.hide();
            splash->show();
            splash->showMessage(splashMessage, splashMessageAlign, splashMessageColor);

            // apps.push_back(startSoftware(
            //     controller->getType(), step_size, data_file, controller, "", OpenCL, test_mode
            // ));
            rhx_app = startSoftware(
                controller->getType(), step_size, data_file, controller, "", OpenCL, test_mode, info
            );

            // splash->finish(apps.pop_back().controlWindow);
            splash->finish(rhx_app.controlWindow);
            boardSelectDialog.accept();
        }
    );

    return app.exec();
}
