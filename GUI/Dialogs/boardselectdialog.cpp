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

#include "boardselectdialog.h"

#include <fmt/format.h>
#include <qboxlayout.h>
#include <qcoreapplication.h>
#include <qdatetime.h>
#include <qeventloop.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qset.h>
#include <qsizepolicy.h>
#include <qstackedlayout.h>
#include <qstackedwidget.h>
#include <qtablewidget.h>
#include <qwidget.h>
#include <qwindowdefs.h>
#include <synchapi.h>
#include <xdaq/device_manager.h>

#include <QApplication>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QtGlobal>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>

#include "../../Engine/API/Hardware/controller_info.h"
#include "abstractrhxcontroller.h"
#include "advancedstartupdialog.h"
#include "datafilereader.h"
#include "playbackrhxcontroller.h"
#include "rhxcontroller.h"
#include "rhxglobals.h"
#include "scrollablemessageboxdialog.h"
#include "startupdialog.h"
#include "syntheticrhxcontroller.h"


using json = nlohmann::json;

namespace fs = std::filesystem;

// Return a QIcon with a picture of the specified board.
QIcon getIcon(XDAQModel model, QStyle *style, int size)
{
    if (model == XDAQModel::Core)
        return QIcon(":/images/xdaq_core.png");
    else if (model == XDAQModel::One)
        return QIcon(":/images/xdaq_one.png");
    else
        return QIcon(style->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(size));
}

// Return a QSize (that should be the minimum size of the table) which allows all columns to be
// visible, and up to 5 rows to be visible before a scroll bar is added.
QSize calculateTableSize(QTableWidget *boardTable)
{
    int width = boardTable->verticalHeader()->width();
    for (int column = 0; column < boardTable->columnCount(); column++) {
        width += boardTable->columnWidth(column);
    }
    width += 4;

    // Make the minimum height to be 5 rows.
    int numRows = 5;
    if (boardTable->rowCount() <= 5) numRows = boardTable->rowCount();
    int height = boardTable->horizontalHeader()->height();
    for (int row = 0; row < numRows; row++) {
        height += boardTable->rowHeight(row);
    }
    height += 4;

    return QSize(width, height);
}

auto create_default_combobox(auto init, const auto &items, auto on_change)
{
    auto combo = new QComboBox();
    for (const auto &item : items) combo->addItem(item);
    combo->setCurrentIndex(init);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), on_change);
    return combo;
}

auto create_default_sample_rate_checkbox = [](QWidget *parent) {
    auto defaultSampleRateCheckBox = new QCheckBox(parent);
    QSettings settings;
    settings.beginGroup("XDAQ");
    defaultSampleRateCheckBox->setChecked(settings.value("useDefaultSettings", false).toBool());
    int defaultSampleRateIndex = settings.value("defaultSampleRate", 14).toInt();
    int defaultStimStepSizeIndex = settings.value("defaultStimStepSize", 6).toInt();
    defaultSampleRateCheckBox->setText(
        parent->tr("Start software with ") + SampleRateString[defaultSampleRateIndex] +
        parent->tr(" sample rate and ") + StimStepSizeString[defaultStimStepSizeIndex]
    );

    QObject::connect(defaultSampleRateCheckBox, &QCheckBox::stateChanged, [](int state) {
        // save the state of the checkbox to QSettings
        QSettings settings;
        settings.beginGroup("XDAQ");
        settings.setValue("useDefaultSettings", state == Qt::Checked);
        settings.endGroup();
    });
    return defaultSampleRateCheckBox;
};

auto create_default_settings_file_checkbox = [](QWidget *parent) {
    auto defaultSettingsFileCheckBox = new QCheckBox(parent);
    QSettings settings;
    settings.beginGroup("XDAQ");
    defaultSettingsFileCheckBox->setChecked(
        settings.value("loadDefaultSettingsFile", false).toBool()
    );
    auto defaultSettingsFile = QString(settings.value("defaultSettingsFile", "").toString());
    defaultSettingsFileCheckBox->setText(
        parent->tr("Load default settings file: ") + defaultSettingsFile
    );
    QObject::connect(defaultSettingsFileCheckBox, &QCheckBox::stateChanged, [](int state) {
        QSettings settings;
        settings.beginGroup("XDAQ");
        settings.setValue("loadDefaultSettingsFile", state == Qt::Checked);
        settings.endGroup();
    });
    return defaultSettingsFileCheckBox;
};

auto get_properties_table(QStringList headers, std::vector<std::vector<QWidget *>> rows)
{
    auto num_rows = rows.size();
    auto num_cols = rows[0].size();
    auto table = new QTableWidget(num_rows, num_cols, nullptr);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionsClickable(false);
    table->verticalHeader()->hide();
    for (int r = 0; r < num_rows; ++r) {
        for (int c = 0; c < num_cols; ++c) {
            table->setCellWidget(r, c, rows[r][c]);
            rows[r][c]->setContentsMargins(2, 0, 2, 0);
        }
    }
    table->resizeColumnsToContents();
    table->resizeRowsToContents();
    table->setMinimumSize(calculateTableSize(table));
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

auto get_playback_board(QWidget *parent, auto launch)
{
    QSettings settings;
    settings.beginGroup("XDAQ");
    auto last_playback_file = settings.value("lastPlaybackFile", "").toString();
    auto last_playback_ports = settings.value("playbackPorts", 255).toUInt();
    settings.endGroup();

    auto app_icon = new QTableWidgetItem(
        parent->style()->standardIcon(QStyle::SP_MediaPlay).pixmap(40), QObject::tr("Playback")
    );

    auto open_file_button = new QPushButton(
        parent->style()->standardIcon(QStyle::SP_DialogOpenButton).pixmap(20),
        last_playback_file.isEmpty() ? QObject::tr("Select Playback Data File") : last_playback_file
    );

    auto launch_button = new QPushButton(
        parent->style()->standardIcon(QStyle::SP_MediaPlay).pixmap(20), parent->tr("Launch")
    );
    launch_button->setEnabled(!last_playback_file.isEmpty());
    // boardTable->setCellWidget(row, 2, launch_button);

    QObject::connect(
        open_file_button,
        &QPushButton::clicked,
        [parent, open_file_button, launch_button]() {
            QSettings settings;
            settings.beginGroup("XDAQ");
            QString playbackFileName = QFileDialog::getOpenFileName(
                nullptr,
                parent->tr("Select Intan Data File"),
                settings.value("playbackDirectory", ".").toString(),  // default directory
                parent->tr("Intan Data Files (*.rhd *.rhs)")
            );
            if (!playbackFileName.isEmpty()) {
                settings.setValue(
                    "playbackDirectory",
                    fs::path(playbackFileName.toStdString()).parent_path().string().c_str()
                );
                settings.setValue("lastPlaybackFile", playbackFileName);
            }
            settings.endGroup();
            open_file_button->setText(playbackFileName);
            launch_button->setEnabled(!playbackFileName.isEmpty());
        }
    );

    std::shared_ptr<json> launch_properties =
        std::shared_ptr<json>(new json{{"playbackPorts", last_playback_ports}});

    std::vector<std::vector<QWidget *>> rows;
    for (int port = 0; port < 8; ++port) {
        auto label = new QLabel(parent->tr(fmt::format("Port {}", (char) ('A' + port)).c_str()));
        auto enable_checkbox = new QCheckBox();
        rows.push_back(
            {label,
             enable_checkbox,
             new QLabel(
                 parent->tr(fmt::format("Enable playback on port {}", (char) ('A' + port)).c_str())
             )}
        );
        enable_checkbox->setChecked((last_playback_ports & (1 << port)) > 0);

        QObject::connect(
            enable_checkbox,
            &QCheckBox::stateChanged,
            [launch_properties = std::weak_ptr<json>(launch_properties), port](int state) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                auto ports = settings.value("playbackPorts", 255).toUInt();
                if (state == Qt::Checked) {
                    ports |= (1 << port);
                } else {
                    ports &= ~(1 << port);
                }
                settings.setValue("playbackPorts", ports);
                settings.endGroup();
                if (auto p = launch_properties.lock()) p->at("playbackPorts") = ports;
            }
        );
    }
    auto launch_properties_widget = get_properties_table(
        {parent->tr("Property"), parent->tr("Value"), parent->tr("Description")}, rows
    );
    auto launch_button_layout = new QVBoxLayout;
    launch_button_layout->addWidget(launch_button, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(new QLabel(parent->tr("Launch Properties")));
    launch_button_layout->addWidget(launch_properties_widget);
    auto launch_button_widget = new QWidget();
    launch_button_widget->setLayout(launch_button_layout);

    QObject::connect(
        launch_button,
        &QPushButton::clicked,
        [parent, open_file_button, launch_button, launch, launch_properties]() mutable {
            auto playbackFileName = open_file_button->text();
            if (!playbackFileName.isEmpty()) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                bool canReadFile = true;
                QString report;
                auto dataFileReader = new DataFileReader(
                    playbackFileName,
                    canReadFile,
                    report,
                    settings.value("playbackPorts", 255).toUInt()
                );

                if (!canReadFile) {
                    ScrollableMessageBoxDialog msgBox(nullptr, "Unable to Load Data File", report);
                    msgBox.exec();
                    settings.endGroup();
                    return;
                } else if (!report.isEmpty()) {
                    ScrollableMessageBoxDialog msgBox(nullptr, "Data File Loaded", report);
                    msgBox.exec();
                    QFileInfo fileInfo(playbackFileName);
                    settings.setValue("playbackDirectory", fileInfo.absolutePath());
                }
                settings.endGroup();
                launch_properties.reset();
                launch(
                    new PlaybackRHXController(
                        dataFileReader->controllerType(),
                        dataFileReader->sampleRate(),
                        dataFileReader
                    ),
                    dataFileReader
                );
            }
        }
    );

    return std::make_tuple(app_icon, open_file_button, launch_button_widget, launch_properties);
}

auto get_xdaq_board(QWidget *parent, auto launch, const XDAQInfo &info, const XDAQStatus &status)
{
    auto app_icon = new QTableWidgetItem(
        getIcon(info.model, parent->style(), info.model == XDAQModel::Unknown ? 40 : 80),
        QString::fromStdString(info.id)
    );
    auto main_layout = new QVBoxLayout;
    main_layout->addWidget(new QLabel(QString::fromStdString(info.plugin)));
    // Report if an io expander is connected.
    if (info.model == XDAQModel::Unknown) {
        main_layout->addWidget(new QLabel(parent->tr("N/A")));
    } else if (info.expander) {
        auto expander_layout = new QHBoxLayout;
        auto icon = new QLabel();
        icon->setPixmap(parent->style()->standardIcon(QStyle::SP_DialogYesButton).pixmap(20));
        expander_layout->addWidget(icon);
        expander_layout->addWidget(new QLabel(parent->tr("I/O Expander Connected")));
        main_layout->addLayout(expander_layout);
    } else {
        auto expander_layout = new QHBoxLayout;
        auto icon = new QLabel();
        icon->setPixmap(parent->style()->standardIcon(QStyle::SP_DialogNoButton).pixmap(20));
        expander_layout->addWidget(icon);
        expander_layout->addWidget(new QLabel(parent->tr("No I/O Expander Connected")));
        main_layout->addLayout(expander_layout);
    }

    // Report the serial number of this board.
    auto serial_layout = new QHBoxLayout;
    serial_layout->addWidget(new QLabel(parent->tr("Serial Number")));
    serial_layout->addWidget(new QLabel(QString::fromStdString(fmt::format("{}", info.serial))));
    main_layout->addLayout(serial_layout);

    auto device_widget = new QWidget();
    device_widget->setLayout(main_layout);
    device_widget->setDisabled(info.model == XDAQModel::Unknown);

    // Show device current mode
    auto mode_layout = new QHBoxLayout;
    mode_layout->addWidget(new QLabel(parent->tr("Current Mode")));
    mode_layout->addWidget(
        status.mode.find("rhd") != string::npos ? new QLabel(QString::fromStdString("X3R/X6R"))
                                                : new QLabel(QString::fromStdString("X3SR"))
    );
    main_layout->addLayout(mode_layout);

    std::shared_ptr<json> launch_properties =
        std::shared_ptr<json>(new json{{"sample_rate", SampleRate30000Hz}, {"stim_step_size", 5}});
    auto launch_button_rhd = new QPushButton(parent->tr("Record (X3R/X6R)"));
    QObject::connect(
        launch_button_rhd,
        &QPushButton::clicked,
        [launch, info, status, launch_properties]() mutable {
            QSettings settings;
            settings.beginGroup("XDAQ");
            auto config = json::parse(info.device_config);
            config["mode"] = "rhd";
            launch(
                new RHXController(
                    ControllerType::ControllerRecordUSB3,
                    launch_properties->at("sample_rate"),
                    info.get_device(config.dump())
                ),
                launch_properties->at("stim_step_size")
            );
            launch_properties.reset();
            settings.endGroup();
        }
    );
    auto launch_button_rhs = new QPushButton(parent->tr("Stim-Record (X3SR)"));
    QObject::connect(
        launch_button_rhs,
        &QPushButton::clicked,
        [launch, info, status, launch_properties]() mutable {
            QSettings settings;
            settings.beginGroup("XDAQ");
            auto config = json::parse(info.device_config);
            config["mode"] = "rhs";
            launch(
                new RHXController(
                    ControllerType::ControllerStimRecord,
                    launch_properties->at("sample_rate"),
                    info.get_device(config.dump())
                ),
                launch_properties->at("stim_step_size")
            );
            launch_properties.reset();
            settings.endGroup();
        }
    );

    auto sr_selector = create_default_combobox(
        SampleRate30000Hz,
        SampleRateString,
        [launch_properties](int index) { launch_properties->at("sample_rate") = index; }
    );
    auto stim_step_selector = create_default_combobox(
        StimStepSize200nA,
        StimStepSizeString,
        [launch_properties](int index) { launch_properties->at("stim_step_size") = index; }
    );

    auto launch_properties_widget = get_properties_table(
        {parent->tr("Property"), parent->tr("Value")},
        {{new QLabel(parent->tr("Sample Rate")), sr_selector

         },
         {new QLabel(parent->tr("Stim Step Size")), stim_step_selector}}
    );


    auto launch_button_layout = new QVBoxLayout;
    launch_button_layout->addWidget(launch_button_rhd, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(launch_button_rhs, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(launch_properties_widget);
    // launch_button_layout->setEnabled(info.model != XDAQModel::Unknown);
    auto launch_button_widget = new QWidget();
    launch_button_widget->setLayout(launch_button_layout);
    return std::make_tuple(app_icon, device_widget, launch_button_widget, launch_properties);
}

auto get_demo_board(QWidget *parent, auto launch)
{
    auto app_icon = new QTableWidgetItem(
        parent->style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(40), parent->tr("Demo")
    );

    std::shared_ptr<json> launch_properties = std::shared_ptr<json>(new json{
        {"sample_rate", SampleRate30000Hz},
        {"stim_step_size", StimStepSize200nA},
    });

    auto sr_selector = create_default_combobox(
        launch_properties->at("sample_rate"),
        SampleRateString,
        [launch_properties](int index) { launch_properties->at("sample_rate") = index; }
    );
    auto stim_step_selector = create_default_combobox(
        launch_properties->at("stim_step_size"),
        StimStepSizeString,
        [launch_properties](int index) { launch_properties->at("stim_step_size") = index; }
    );

    auto launch_properties_widget = get_properties_table(
        {parent->tr("Property"), parent->tr("Value")},
        {{new QLabel(parent->tr("Sample Rate")), sr_selector},
         {new QLabel(parent->tr("Stim Step Size")), stim_step_selector}}
    );

    auto launch_button_rhd = new QPushButton(parent->tr("Record (X3R/X6R) Demo"));
    QObject::connect(launch_button_rhd, &QPushButton::clicked, [launch, launch_properties]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        launch(
            new SyntheticRHXController(ControllerType::ControllerRecordUSB3, SampleRate30000Hz),
            launch_properties->at("stim_step_size")
        );
        settings.endGroup();
    });
    auto launch_button_rhs = new QPushButton(parent->tr("Stim-Record (X3SR) Demo"));
    QObject::connect(launch_button_rhs, &QPushButton::clicked, [launch, launch_properties]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        launch(
            new SyntheticRHXController(ControllerType::ControllerStimRecord, SampleRate30000Hz),
            launch_properties->at("stim_step_size")
        );
        settings.endGroup();
    });
    auto launch_button_layout = new QVBoxLayout;
    launch_button_layout->addWidget(launch_button_rhd, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(launch_button_rhs, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(launch_properties_widget);
    auto launch_button_widget = new QWidget();
    launch_button_widget->setLayout(launch_button_layout);
    // boardTable->setCellWidget(row, 2, launch_button_widget);
    return std::make_tuple(app_icon, new QWidget(), launch_button_widget, launch_properties);
}

std::vector<std::shared_ptr<xdaq::DeviceManager>> get_device_managers()
{
#ifdef _WIN32
    auto app_dir = fs::path(QCoreApplication::applicationDirPath().toStdString());
    auto app_manager_dir = app_dir / "managers";
    if (!fs::exists(app_manager_dir)) {
        return {};
    }
    std::unordered_set<fs::path> search_paths;
    for (auto &path : fs::directory_iterator(app_manager_dir)) {
        search_paths.insert(fs::canonical(fs::path(path)));
    }
    // fmt::println("app_dir: {}", app_dir.generic_string());
    // constexpr auto extension = ".dll";
    // auto res = SetDllDirectoryA((app_dir / "managers").generic_string().c_str());
    // if (res == 0) throw std::runtime_error("Failed to set DLL directory");
    // std::cout << "res = " << res << std::endl;
#elif __APPLE__
    const std::vector<fs::path> search_path = {"/usr/local/lib/xdaq/plugins", "./plugins"};
    constexpr auto extension = ".dylib";
#elif __linux__
    const std::vector<fs::path> search_path = {"/usr/local/lib/xdaq/plugins", "./plugins"};
    constexpr auto extension = ".so";
#else
    static_assert(false, "Unsupported platform");
#endif
    // auto plugin_paths =
    //     fs::directory_iterator((app_dir / "managers")) |
    //     std::views::filter([=](const fs::directory_entry &entry) {
    //         fmt::println("Checking: {}", entry.path().generic_string());
    //         if (fs::is_directory(entry)) return false;
    //         if (!entry.path().filename().generic_string().contains("device_manager")) return false;
    //         return entry.path().extension() == extension;
    //     }) |
    //     std::views::transform([](auto ent) { return fs::canonical(fs::path(ent)); }) |
    //     std::ranges::to<std::vector>();
    // std::ranges::sort(plugin_paths);
    // plugin_paths.erase(std::unique(plugin_paths.begin(), plugin_paths.end()), plugin_paths.end());

    // std::vector<std::shared_ptr<xdaq::DeviceManager>> plugins;

    // for (const auto &path : plugin_paths) {
    //     try {
    //         auto plugin = xdaq::get_manager(path.generic_string());
    //         plugins.emplace_back(plugin);
    //     } catch (...) {
    //     }
    // }
    // return plugins;
    std::vector<std::shared_ptr<xdaq::DeviceManager>> device_managers;

    for (const auto &path : search_paths) {
        try {
            device_managers.emplace_back(xdaq::get_manager(path));
        } catch (...) {
        }
    }
    return device_managers;
}

// void BoardSelectDialog::ScanDevice()
auto ScanDevice()
{
    std::vector<XDAQInfo> controllers_info;
    std::vector<XDAQStatus> controllers_status;

    auto plugins = get_device_managers();

    for (auto &plugin : plugins) {
        fmt::println("Plugin: {}", plugin->get_device_options());
        auto devices = json::parse(plugin->list_devices());
        fmt::println("Device: {}", plugin->list_devices());

        for (auto &device : devices) {
            device["mode"] = "rhd";
            auto dev = plugin->create_device(device.dump());
            auto status = json::parse(*dev->get_status());
            auto info = json::parse(*dev->get_info());
            auto xdaq_status = parse_status(status);
            auto xdaq_info = parse_info(info);

            // if (std::find(controller_info.begin(), controller_info.end(), xdaq_info) !=
            //     controller_info.end()) {
            //     fmt::println("Device: {} already existed", xdaq_info.serial);
            //     continue;
            // }
            xdaq_info.plugin = json::parse(plugin->info()).at("name");
            xdaq_info.device_config = device.dump();
            xdaq_info.get_device = [device, plugin](const std::string &config) {
                return plugin->create_device(config);
            };

            controllers_info.emplace_back(xdaq_info);
            controllers_status.emplace_back(xdaq_status);
        }
    }
    return std::make_tuple(controllers_info, controllers_status);
}

// auto insert_board(StackedWidget *launch_panel, QTableWidget *boardTable,
// std::vector<std::shared_ptr<json>> board_launch_properties, auto &&board)
auto insert_board(StackedWidget *launch_panel, QTableWidget *boardTable, auto &&board)
{
    auto [app_icon, device_widget, launch_button_widget, launch_properties] = board;
    auto row = boardTable->rowCount();
    boardTable->insertRow(row);
    boardTable->setItem(row, 0, app_icon);
    boardTable->setCellWidget(row, 1, device_widget);
    // Refresh table contents
    boardTable->resizeColumnsToContents();
    boardTable->resizeRowsToContents();

    launch_button_widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    launch_panel->addWidget(launch_button_widget);
    // board_launch_properties.emplace_back(launch_properties);
    
}

// void RemoveBoard(StackedWidget *launch_panel, QTableWidget *boardTable,
//     std::vector<std::shared_ptr<json>> &board_launch_properties)
// {
//     // for (int i = 0; i < boardTable->rowCount(); ++i) {
//     //     // board_launch_properties.erase(board_launch_properties.begin() + i);
//     //     boardTable->removeCellWidget(i, 1);
//     //     boardTable->removeRow(i);
//     // }
//     // boardTable->model()->removeRows(0, boardTable->rowCount());
//     boardTable->setRowCount(0);

//     board_launch_properties.clear();

//     // Reset before scanning devices
//     controller_info.clear();
//     controller_status.clear();
// }

void InsertBoard(
    BoardSelectDialog *parent, StackedWidget *launch_panel, QTableWidget *boardTable,
    std::vector<std::shared_ptr<json>> board_launch_properties
)
{
    insert_board(
        launch_panel,
        boardTable,
        // board_launch_properties,
        get_playback_board(
            parent,
            [parent](AbstractRHXController *controller, DataFileReader *data_file) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                auto use_opencl = settings.value("useOpenCL", true).toBool();
                settings.endGroup();
                parent->emit launch(
                    controller, data_file->stimStepSize(), data_file, use_opencl, false, {}
                );
            }
        )
    );

    // ScanDevice();
    auto [controllers_info, controllers_status] = ScanDevice();
    for (auto info_status : std::ranges::views::zip(controllers_info, controllers_status)) {
        auto info = std::get<0>(info_status);
        auto status = std::get<1>(info_status);

        insert_board(
            launch_panel,
            boardTable,
            // board_launch_properties,
            get_xdaq_board(
                parent,
                [parent, info](AbstractRHXController *controller, StimStepSize step_size) {
                    QSettings settings;
                    settings.beginGroup("XDAQ");
                    auto use_opencl = settings.value("useOpenCL", true).toBool();
                    settings.endGroup();
                    parent->emit launch(controller, step_size, nullptr, use_opencl, false, info);
                },
                info,
                status
            )
        );
    }

    insert_board(
        launch_panel,
        boardTable,
        // board_launch_properties,
        get_demo_board(
            parent,
            [parent](AbstractRHXController *controller, StimStepSize step_size) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                auto use_opencl = settings.value("useOpenCL", true).toBool();
                settings.endGroup();
                parent->emit launch(controller, step_size, nullptr, use_opencl, false, {});
            }
        )
    );
}

// Create a dialog window for user to select which board's software to initialize.
BoardSelectDialog::BoardSelectDialog(QWidget *parent) : QDialog(parent)
{
    auto launch_panel = new StackedWidget(parent);
    auto boardTable = new QTableWidget(0, 2, parent);
    // Set up header.
    boardTable->setHorizontalHeaderLabels({tr("App"), tr("Info")});
    boardTable->horizontalHeader()->setSectionsClickable(false);
    boardTable->verticalHeader()->setSectionsClickable(false);
    boardTable->setFocusPolicy(Qt::ClickFocus);

    InsertBoard(this, launch_panel, boardTable, board_launch_properties);

    // Make table visible in full (for up to 5 rows... then allow a scroll bar to be used).
    boardTable->setIconSize(QSize(283, 100));
    boardTable->resizeColumnsToContents();
    boardTable->resizeRowsToContents();
    boardTable->setMinimumSize(calculateTableSize(boardTable));
    boardTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    boardTable->setSelectionMode(QAbstractItemView::SingleSelection);

    launch_panel->addWidget(new QLabel(tr("Select a board to launch")));    
    launch_panel->setCurrentIndex(launch_panel->count() - 1);
    connect(boardTable, &QTableWidget::currentCellChanged, [this, launch_panel, boardTable]() {
        auto current_row = boardTable->currentRow();
        if (current_row == -1) return;
        launch_panel->currentWidget()->hide();
        launch_panel->setCurrentIndex(current_row);
        launch_panel->resize(launch_panel->currentWidget()->sizeHint());
        launch_panel->currentWidget()->show();
        this->resize(this->sizeHint());
    });

    // Allow the user to open 'Advanced' dialog to allow opting out of OpenCL
    auto advancedButton = new QPushButton(tr("Advanced"), this);
    advancedButton->setFixedWidth(advancedButton->sizeHint().width() + 10);
    connect(advancedButton, &QPushButton::clicked, this, [&]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        bool use_opencl = settings.value("useOpenCL", true).toBool();
        std::uint8_t playback_ports = 255;
        AdvancedStartupDialog advancedStartupDialog(use_opencl, playback_ports, false, this);
        advancedStartupDialog.exec();
        settings.setValue("useOpenCL", use_opencl);
        settings.setValue("playbackPorts", playback_ports);
        settings.endGroup();
    });

    auto rescanDeviceButton = new QPushButton(tr("Rescan Devices"), this);
    rescanDeviceButton->setFixedWidth(rescanDeviceButton->sizeHint().width() + 10);
    connect(rescanDeviceButton, &QPushButton::clicked, this, [this, launch_panel, boardTable]() {
        // RemoveBoard(launch_panel, boardTable, board_launch_properties, this);
        
        // launch_panel->removeWidget(launch_panel);
        // launch_panel->removeWidget(launch_panel->currentWidget());
        for(int i = launch_panel->count(); i >= 0; i--)
        {
            QWidget* widget = launch_panel->widget(i);
            launch_panel->removeWidget(widget);
            widget->deleteLater();
        }
        
        // boardTable->clearSelection();
        // boardTable->disconnect();
        // boardTable->clearContents();
        boardTable->setRowCount(0);

        // board_launch_properties.clear();

        // Reset before scanning devices
        // controllers_info.clear();
        // controllers_status.clear();


        InsertBoard(this, launch_panel, boardTable, board_launch_properties);

        // QTime dieTime = QTime::currentTime().addSecs(1);
        // while (QTime::currentTime() < dieTime)
        //     QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    });

    auto mainLayout = new QVBoxLayout;
    auto boardsLayout = new QHBoxLayout;
    auto buttonsLayout = new QHBoxLayout;

    boardsLayout->addWidget(boardTable);
    boardsLayout->addWidget(launch_panel);
    buttonsLayout->addWidget(advancedButton, 0, Qt::AlignLeft);
    buttonsLayout->addWidget(rescanDeviceButton, 1, Qt::AlignLeft);

    mainLayout->addLayout(boardsLayout);
    mainLayout->addWidget(create_default_settings_file_checkbox(parent));
    mainLayout->addWidget(create_default_sample_rate_checkbox(parent));
    mainLayout->addLayout(buttonsLayout);

    setWindowTitle("Select XDAQ");
    setLayout(mainLayout);
}