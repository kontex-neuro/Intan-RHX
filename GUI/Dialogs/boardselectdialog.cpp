//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
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
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qwindowdefs.h>
#include <xdaq/device_manager.h>

#include <QBoxLayout>
#include <QCoreApplication>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>
#include <QtGlobal>
#include <bitset>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>
#include <unordered_set>

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
    else if (model == XDAQModel::AIO)
        return QIcon(":/images/xdaq_one.png");  // TODO: Add AIO image
    else
        return QIcon(style->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(size));
}

QSize calculateTableSize(QTableWidget *boardTable, const int min_rows, const int max_rows)
{
    int width = boardTable->verticalHeader()->width();
    for (int column = 0; column < boardTable->columnCount(); column++) {
        width += boardTable->columnWidth(column);
    }
    width += 4;

    int numRows = min_rows;  // Set minimum height by rows
    if (boardTable->rowCount() <= max_rows) numRows = boardTable->rowCount();
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

    QObject::connect(
        defaultSampleRateCheckBox,
        &QCheckBox::checkStateChanged,
        [](Qt::CheckState state) {
            // save the state of the checkbox to QSettings
            QSettings settings;
            settings.beginGroup("XDAQ");
            settings.setValue("useDefaultSettings", state == Qt::Checked);
            settings.endGroup();
        }
    );
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
    QObject::connect(
        defaultSettingsFileCheckBox,
        &QCheckBox::checkStateChanged,
        [](Qt::CheckState state) {
            QSettings settings;
            settings.beginGroup("XDAQ");
            settings.setValue("loadDefaultSettingsFile", state == Qt::Checked);
            settings.endGroup();
        }
    );
    return defaultSettingsFileCheckBox;
};

auto get_properties_table(QStringList headers, std::vector<std::vector<QWidget *>> rows)
{
    auto num_rows = rows.size();
    auto num_cols = num_rows > 0 ? rows[0].size() : 0;
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
    table->setMinimumSize(calculateTableSize(table, 10, 10));
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
            &QCheckBox::checkStateChanged,
            [port](Qt::CheckState state) {
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
        [parent, open_file_button, launch_button, launch]() {
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
                launch(
                    [dataFileReader]() {
                        return new PlaybackRHXController(
                            dataFileReader->controllerType(),
                            dataFileReader->sampleRate(),
                            dataFileReader
                        );
                    },
                    dataFileReader
                );
            }
        }
    );

    return std::make_tuple(app_icon, open_file_button, launch_button_widget);
}

auto get_xdaq_board(QWidget *parent, auto launch, const XDAQInfo &info, const XDAQStatus &status)
{
    std::string settings_scope;
    settings_scope = "XDAQ";
    switch (info.model) {
    case XDAQModel::Core: settings_scope += "/Core"; break;
    case XDAQModel::One: settings_scope += "/One"; break;
    case XDAQModel::AIO: settings_scope += "/AIO"; break;
    default: break;
    }
    settings_scope += "/" + info.serial;

    auto app_icon = new QTableWidgetItem(
        getIcon(info.model, parent->style(), info.model == XDAQModel::Unknown ? 40 : 80),
        QString::fromStdString(info.id)
    );

    auto device_widget = new QWidget();
    {
        auto main_layout = new QVBoxLayout;
        main_layout->addWidget(new QLabel(QString::fromStdString(info.plugin)));
        // Report if an io expander is connected.
        if (info.model == XDAQModel::Unknown) {
            main_layout->addWidget(new QLabel(parent->tr("N/A")));
        } else if (status.expander) {
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
        serial_layout->addWidget(new QLabel(QString::fromStdString(info.serial)));
        main_layout->addLayout(serial_layout);
        device_widget->setLayout(main_layout);
        device_widget->setDisabled(info.model == XDAQModel::Unknown);

        // Show device current mode
        auto mode_layout = new QHBoxLayout;
        mode_layout->addWidget(new QLabel(parent->tr("Current Mode")));
        if (status.mode == "rhd")
            mode_layout->addWidget(new QLabel(QString::fromStdString("X3R/X6R")));
        else if (status.mode == "rhs")
            mode_layout->addWidget(new QLabel(QString::fromStdString("X3SR")));
        else if (status.mode == "np")
            mode_layout->addWidget(new QLabel(QString::fromStdString("NeuroPixel")));
        else if (status.mode == "bootloader")
            mode_layout->addWidget(new QLabel(QString::fromStdString("Bootloader")));

        main_layout->addLayout(mode_layout);
    }

    auto launch_widget = new QWidget();
    {
        QSettings settings;
        settings.beginGroup(settings_scope);
        if (!settings.contains("rhd/sample_rate"))
            settings.setValue("rhd/sample_rate", SampleRateString[SampleRate30000Hz]);
        if (!settings.contains("rhs/sample_rate"))
            settings.setValue("rhs/sample_rate", SampleRateString[SampleRate30000Hz]);
        if (!settings.contains("rhs/stim_step_size"))
            settings.setValue("rhs/stim_step_size", StimStepSizeString[StimStepSize10uA]);

        const static auto dump_qsettings = [](QString group) {
            QSettings settings;
            settings.beginGroup(group);
            fmt::print("[{}]\n", group.toStdString());
            for (const auto &key : settings.allKeys()) {
                fmt::print(
                    "{}={}\n", key.toStdString(), settings.value(key).toString().toStdString()
                );
            }
            settings.endGroup();
        };
        dump_qsettings("XDAQ");
        std::vector<QString> rhd_sample_rates;
        for (int i = 0; i <= SampleRate30000Hz; ++i)
            rhd_sample_rates.push_back(SampleRateString[i]);
        auto rhd_sr_selector = create_default_combobox(
            std::ranges::find(rhd_sample_rates, settings.value("rhd/sample_rate").toString()) -
                rhd_sample_rates.begin(),
            rhd_sample_rates,
            [rhd_sample_rates, settings_scope](int index) {
                QSettings settings;
                settings.setValue(settings_scope + "/rhd/sample_rate", rhd_sample_rates[index]);
            }
        );
        std::vector<std::vector<QWidget *>> rhd_rows{
            {{new QLabel(parent->tr("Sample Rate")), rhd_sr_selector}}
        };
        constexpr static auto get_port_name = [](int idx, bool rhs) {
            if (rhs) {
                if (idx & 1) {
                    return fmt::format("HDMI {} RHX {} 16-31", idx / 2, (char) ('A' + idx / 2));
                } else {
                    return fmt::format("HDMI {} RHX {} 0-15", idx / 2, (char) ('A' + idx / 2));
                }
            } else {
                if (idx & 1) {
                    return fmt::format("HDMI {} RHX {} 128-255", idx / 2, (char) ('A' + idx));
                } else {
                    return fmt::format("HDMI {} RHX {} 0-127", idx / 2, (char) ('A' + idx));
                }
            }
        };
        if (info.generation == 2) {
            std::bitset<8> enabled;
            int rhd_available_ports = info.max_rhd_channels / 128;
            for (int port = 0; port < 8; ++port) {
                const auto port_name = get_port_name(port, false);
                auto e = settings.value("rhd/" + port_name, false).toBool();
                if (e && (enabled.count() >= rhd_available_ports)) {
                    e = false;
                    settings.setValue("rhd/" + port_name, false);
                }
                enabled[port] = e;
            }
            if (enabled.count() == 0) {
                for (int port = 0; port < 8; ++port) {
                    if ((info.model == XDAQModel::Core) && (port & 1)) continue;
                    if (enabled.count() < rhd_available_ports) {
                        const auto port_name = get_port_name(port, false);
                        settings.setValue("rhd/" + port_name, true);
                        enabled[port] = true;
                    }
                }
            }
            std::vector<QCheckBox *> checkboxes;
            for (int port = 0; port < 8; ++port) {
                if ((info.model == XDAQModel::Core) && (port & 1)) continue;
                const auto port_name = get_port_name(port, false);
                auto label = new QLabel(parent->tr(port_name.c_str()));

                auto enable_checkbox = new QCheckBox();
                enable_checkbox->setCheckState(enabled[port] ? Qt::Checked : Qt::Unchecked);
                enable_checkbox->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; }"
                );
                rhd_rows.push_back({label, enable_checkbox});
                checkboxes.push_back(enable_checkbox);
            }

            for (int port = 0, count = 0; port < 8; ++port) {
                if ((info.model == XDAQModel::Core) && (port & 1)) continue;
                const auto port_name = get_port_name(port, false);
                auto target = checkboxes[count];
                ++count;
                QObject::connect(
                    target,
                    &QCheckBox::checkStateChanged,
                    [info, settings_scope, port_name, checkboxes, target](Qt::CheckState state) {
                        int rhd_available_ports = info.max_rhd_channels / 128;
                        for (auto cb : checkboxes) {
                            if (cb->checkState() == Qt::Checked) {
                                --rhd_available_ports;
                            }
                        }
                        QSettings settings;
                        if (state == Qt::Checked) {
                            if (rhd_available_ports < 0) {
                                fmt::print("no more available ports {}\n", rhd_available_ports);
                                target->setCheckState(Qt::Unchecked);
                                return;
                            }
                            settings.setValue(settings_scope + "/rhd/" + port_name, true);
                        } else {
                            settings.setValue(settings_scope + "/rhd/" + port_name, false);
                        }
                    }
                );
            }
        }
        auto rhd_properties_widget =
            get_properties_table({parent->tr("Property"), parent->tr("Value")}, rhd_rows);
        const std::vector<QString> rhs_sample_rates = {
            SampleRateString[SampleRate20000Hz],
            SampleRateString[SampleRate25000Hz],
            SampleRateString[SampleRate30000Hz],
        };
        if (std::ranges::find(rhs_sample_rates, settings.value("rhs/sample_rate").toString()) ==
            rhs_sample_rates.end()) {
            settings.setValue("rhs/sample_rate", SampleRateString[SampleRate30000Hz]);
        }
        auto rhs_sr_selector = create_default_combobox(
            std::ranges::find(rhs_sample_rates, settings.value("rhs/sample_rate").toString()) -
                rhs_sample_rates.begin(),
            rhs_sample_rates,
            [rhs_sample_rates, settings_scope](int index) {
                QSettings settings;
                settings.setValue(settings_scope + "/rhs/sample_rate", rhs_sample_rates[index]);
            }
        );
        std::vector<QString> rhs_stim_step_sizes;
        for (int i = 0; i <= StimStepSizeMax; ++i)
            rhs_stim_step_sizes.push_back(StimStepSizeString[i]);

        if (std::ranges::find(
                rhs_stim_step_sizes, settings.value("rhs/stim_step_size").toString()
            ) == rhs_stim_step_sizes.end()) {
            settings.setValue("rhs/stim_step_size", StimStepSizeString[StimStepSize10uA]);
        }
        auto stim_step_selector = create_default_combobox(
            std::ranges::find(
                rhs_stim_step_sizes, settings.value("rhs/stim_step_size").toString()
            ) - rhs_stim_step_sizes.begin(),
            StimStepSizeString,
            [settings_scope](int index) {
                QSettings settings;
                settings.setValue(
                    settings_scope + "/rhs/stim_step_size", StimStepSizeString[index]
                );
            }
        );
        std::vector<std::vector<QWidget *>> rhs_rows{
            {new QLabel(parent->tr("Sample Rate")), rhs_sr_selector},
            {new QLabel(parent->tr("Stim Step Size")), stim_step_selector}
        };
        if (info.generation == 2) {
            std::bitset<8> enabled;
            int rhs_available_ports = info.max_rhs_channels / 16;
            for (int port = 0; port < 8; ++port) {
                const auto port_name = get_port_name(port, true);

                auto e = settings.value("rhs/" + port_name, false).toBool();
                if (e && (enabled.count() >= rhs_available_ports)) {
                    e = false;
                    settings.setValue("rhs/" + port_name, false);
                }
                enabled[port] = e;
            }
            if (enabled.count() == 0) {
                for (int port = 0; port < 8; ++port) {
                    if (enabled.count() < rhs_available_ports) {
                        const auto port_name = get_port_name(port, true);
                        settings.setValue("rhs/" + port_name, true);
                        enabled[port] = true;
                    }
                }
            }
            std::vector<QCheckBox *> checkboxes;
            for (int port = 0; port < 8; ++port) {
                if ((info.model == XDAQModel::Core) && (port >= 4)) continue;
                const auto port_name = get_port_name(port, true);
                auto enable_checkbox = new QCheckBox();
                enable_checkbox->setCheckState(enabled[port] ? Qt::Checked : Qt::Unchecked);
                enable_checkbox->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; }"
                );
                rhs_rows.push_back({new QLabel(parent->tr(port_name.c_str())), enable_checkbox});
                checkboxes.push_back(enable_checkbox);
            }

            for (int port = 0, count = 0; port < 8; ++port) {
                if ((info.model == XDAQModel::Core) && (port >= 4)) continue;
                const auto port_name = get_port_name(port, true);
                auto target = checkboxes[count];
                ++count;
                QObject::connect(
                    target,
                    &QCheckBox::checkStateChanged,
                    [info, settings_scope, port_name, checkboxes, port, target](Qt::CheckState state
                    ) {
                        int rhs_available_ports = info.max_rhs_channels / 16;
                        for (auto cb : checkboxes) {
                            if (cb->checkState() == Qt::Checked) {
                                --rhs_available_ports;
                            }
                        }
                        QSettings settings;
                        if (state == Qt::Checked) {
                            if (rhs_available_ports < 0) {
                                fmt::print("no more available ports {}\n", rhs_available_ports);
                                target->setCheckState(Qt::Unchecked);
                                return;
                            }
                            settings.setValue(settings_scope + "/rhs/" + port_name, true);
                        } else {
                            settings.setValue(settings_scope + "/rhs/" + port_name, false);
                        }
                    }
                );
            }
        }
        auto rhs_properties_widget =
            get_properties_table({parent->tr("Property"), parent->tr("Value")}, rhs_rows);

        auto tab = new QTabWidget();
        tab->addTab(rhd_properties_widget, parent->tr("Record"));
        tab->addTab(rhs_properties_widget, parent->tr("Stim-Record"));
        auto launch_mode = settings.value("launch_mode", "rhd").toString();
        QPushButton *launch_button;
        if (launch_mode == "rhd") {
            tab->setCurrentIndex(0);
            launch_button = new QPushButton(parent->tr("Launch (Record)"));
        } else {
            tab->setCurrentIndex(1);
            launch_button = new QPushButton(parent->tr("Launch (Stim-Record)"));
        }
        QObject::connect(
            tab,
            &QTabWidget::currentChanged,
            [launch_button, settings_scope, tab](int index) {
                QSettings settings;
                if (index == 0) {
                    settings.setValue(settings_scope + "/launch_mode", "rhd");
                    launch_button->setText(QObject::tr("Launch (Record)"));
                } else {
                    settings.setValue(settings_scope + "/launch_mode", "rhs");
                    launch_button->setText(QObject::tr("Launch (Stim-Record)"));
                }
            }
        );

        QObject::connect(
            launch_button,
            &QPushButton::clicked,
            [launch, info, tab, settings_scope, rhd_sample_rates, rhs_stim_step_sizes]() {
                auto config = json::parse(info.device_config);
                QSettings settings;
                settings.beginGroup(settings_scope);
                auto launch_mode = settings.value("launch_mode", "rhd").toString();
                settings.beginGroup(launch_mode);
                std::bitset<8> enabled;
                if (info.generation == 2) {
                    for (int port = 0; port < 8; ++port) {
                        const auto port_name = get_port_name(port, launch_mode == "rhs");
                        auto e = settings.value(port_name, false).toBool();
                        enabled[port] = e;
                    }
                }

                auto sample_rate = settings.value("sample_rate").toString();
                auto sample_rate_it = std::ranges::find(rhd_sample_rates, sample_rate);
                if (sample_rate_it == rhd_sample_rates.end()) {
                    QMessageBox::warning(
                        nullptr,
                        "Invalid Sample Rate",
                        fmt::format("Sample rate {} is not supported", sample_rate.toStdString())
                            .c_str()
                    );
                    return;
                }
                auto sample_rate_enum =
                    static_cast<AmplifierSampleRate>(sample_rate_it - rhd_sample_rates.begin());

                if (launch_mode == "rhd") {
                    config["mode"] = "rhd";

                    launch(
                        [=]() {
                            auto device = info.get_device(config.dump());
                            if (info.generation == 2) {
                                device->set_register_sync(0x1004u, enabled.to_ulong());
                            }
                            return new RHXController(
                                ControllerType::ControllerRecordUSB3,
                                sample_rate_enum,
                                std::move(device)
                            );
                        },
                        StimStepSize::StimStepSizeUnrecognized
                    );
                } else {
                    config["mode"] = "rhs";

                    auto stim_step_size = settings.value("stim_step_size").toString();
                    auto stim_step_size_it = std::ranges::find(rhs_stim_step_sizes, stim_step_size);
                    if (stim_step_size_it == rhs_stim_step_sizes.end()) {
                        QMessageBox::warning(
                            nullptr,
                            "Invalid Stim Step Size",
                            fmt::format(
                                "Stim step size {} is not supported", stim_step_size.toStdString()
                            )
                                .c_str()
                        );
                        return;
                    }
                    auto stim_step_size_enum =
                        static_cast<StimStepSize>(stim_step_size_it - rhs_stim_step_sizes.begin());

                    launch(
                        [=]() {
                            auto device = info.get_device(config.dump());
                            if (info.generation == 2) {
                                device->set_register_sync(0x1004u, enabled.to_ulong());
                            }
                            return new RHXController(
                                ControllerType::ControllerStimRecord,
                                sample_rate_enum,
                                std::move(device)
                            );
                        },
                        stim_step_size_enum
                    );
                }
            }
        );


        auto launch_button_layout = new QVBoxLayout;
        launch_button_layout->addWidget(launch_button);
        launch_button_layout->addWidget(tab);
        launch_button_layout->setEnabled(info.model != XDAQModel::Unknown);
        launch_widget->setLayout(launch_button_layout);
    }


    return std::make_tuple(app_icon, device_widget, launch_widget);
}

auto get_demo_board(QWidget *parent, auto launch)
{
    auto app_icon = new QTableWidgetItem(
        parent->style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(40), parent->tr("Demo")
    );

    QSettings settings;
    settings.beginGroup("XDAQ");
    if (!settings.contains("demo_sample_rate"))
        settings.setValue("demo_sample_rate", SampleRate30000Hz);
    if (!settings.contains("demo_stim_step_size"))
        settings.setValue("demo_stim_step_size", StimStepSize10uA);
    auto init_sample_rate =
        static_cast<AmplifierSampleRate>(settings.value("demo_sample_rate").toInt());
    auto init_stim_step_size =
        static_cast<StimStepSize>(settings.value("demo_stim_step_size").toInt());
    settings.endGroup();

    auto sr_selector = create_default_combobox(init_sample_rate, SampleRateString, [](int index) {
        QSettings settings;
        settings.beginGroup("XDAQ");
        settings.setValue("demo_sample_rate", index);
        settings.endGroup();
    });
    auto stim_step_selector =
        create_default_combobox(init_stim_step_size, StimStepSizeString, [](int index) {
            QSettings settings;
            settings.beginGroup("XDAQ");
            settings.setValue("demo_stim_step_size", index);
            settings.endGroup();
        });

    auto launch_properties_widget = get_properties_table(
        {parent->tr("Property"), parent->tr("Value")},
        {{new QLabel(parent->tr("Sample Rate")), sr_selector},
         {new QLabel(parent->tr("Stim Step Size")), stim_step_selector}}
    );

    auto launch_button_rhd = new QPushButton(parent->tr("Record (X3R/X6R) Demo"));
    QObject::connect(launch_button_rhd, &QPushButton::clicked, [launch]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        auto sample_rate =
            static_cast<AmplifierSampleRate>(settings.value("demo_sample_rate").toInt());
        auto stim_step_size =
            static_cast<StimStepSize>(settings.value("demo_stim_step_size").toInt());
        settings.endGroup();

        launch(
            [=]() {
                return new SyntheticRHXController(
                    ControllerType::ControllerRecordUSB3, sample_rate
                );
            },
            stim_step_size
        );
    });
    auto launch_button_rhs = new QPushButton(parent->tr("Stim-Record (X3SR) Demo"));
    QObject::connect(launch_button_rhs, &QPushButton::clicked, [launch]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        auto sample_rate =
            static_cast<AmplifierSampleRate>(settings.value("demo_sample_rate").toInt());
        auto stim_step_size =
            static_cast<StimStepSize>(settings.value("demo_stim_step_size").toInt());
        settings.endGroup();
        if (AbstractRHXController::getSampleRate(sample_rate) <
            AbstractRHXController::getSampleRate(SampleRate20000Hz)) {
            QMessageBox::warning(
                nullptr,
                "Unsupported Sample Rate",
                "Only 20, 25 and 30 kHz is supported using Stim-Record"
            );
            return;
        }
        launch(
            [=]() {
                return new SyntheticRHXController(
                    ControllerType::ControllerStimRecord, sample_rate
                );
            },
            stim_step_size
        );
    });
    auto launch_button_layout = new QVBoxLayout;
    launch_button_layout->addWidget(launch_button_rhd, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(launch_button_rhs, 0, Qt::AlignLeft);
    launch_button_layout->addWidget(launch_properties_widget);
    auto launch_button_widget = new QWidget();
    launch_button_widget->setLayout(launch_button_layout);
    // boardTable->setCellWidget(row, 2, launch_button_widget);
    return std::make_tuple(app_icon, new QWidget(), launch_button_widget);
}

std::vector<std::shared_ptr<xdaq::DeviceManager>> get_device_managers()
{
    auto app_dir = fs::path(QCoreApplication::applicationDirPath().toStdString());
#ifdef __APPLE__
    auto app_manager_dir = app_dir / ".." / "PlugIns" / "managers";
#else
    auto app_manager_dir = app_dir / "managers";
#endif
    if (!fs::exists(app_manager_dir)) {
        return {};
    }
    std::unordered_set<fs::path> search_paths;
    for (auto &path : fs::directory_iterator(app_manager_dir)) {
        search_paths.insert(fs::canonical(fs::path(path)));
    }

    std::vector<std::shared_ptr<xdaq::DeviceManager>> device_managers;

    for (const auto &path : search_paths) {
        try {
            device_managers.emplace_back(xdaq::get_device_manager(path));
        } catch (...) {
        }
    }
    return device_managers;
}

auto ScanDevice()
{
    std::vector<XDAQInfo> controllers_info;
    std::vector<XDAQStatus> controllers_status;

    auto plugins = get_device_managers();

    for (auto &plugin : plugins) {
        auto devices = json::parse(plugin->list_devices());

        for (auto &device : devices) {
            auto dev = plugin->create_device(device.dump());
            auto status = json::parse(*dev->get_status());
            auto info = json::parse(*dev->get_info());
            auto xdaq_status = parse_status(status);
            auto xdaq_info = parse_info(info);

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

auto insert_board(StackedWidget *launch_panel, QTableWidget *boardTable, auto &&board)
{
    auto [app_icon, device_widget, launch_button_widget] = board;
    auto row = boardTable->rowCount();
    boardTable->insertRow(row);
    boardTable->setItem(row, 0, app_icon);
    boardTable->setCellWidget(row, 1, device_widget);
    launch_button_widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    launch_panel->addWidget(launch_button_widget);
    // board_launch_properties.emplace_back(launch_properties);
}

void InsertBoard(BoardSelectDialog *parent, StackedWidget *launch_panel, QTableWidget *boardTable)
{
    insert_board(
        launch_panel,
        boardTable,
        get_playback_board(
            parent,
            [parent](
                std::function<AbstractRHXController *()> open_controller, DataFileReader *data_file
            ) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                auto use_opencl = settings.value("useOpenCL", true).toBool();
                settings.endGroup();
                parent->emit launch(
                    open_controller,
                    data_file->stimStepSize(),
                    data_file,
                    use_opencl,
                    false,
                    true,
                    false,
                    2
                );
            }
        )
    );

    auto [controllers_info, controllers_status] = ScanDevice();
    for (auto info_status : std::ranges::views::zip(controllers_info, controllers_status)) {
        auto info = std::get<0>(info_status);
        auto status = std::get<1>(info_status);

        insert_board(
            launch_panel,
            boardTable,
            get_xdaq_board(
                parent,
                [parent, info, status](
                    std::function<AbstractRHXController *()> open_controller, StimStepSize step_size
                ) {
                    QSettings settings;
                    settings.beginGroup("XDAQ");
                    auto use_opencl = settings.value("useOpenCL", true).toBool();
                    settings.endGroup();
                    parent->emit launch(
                        std::move(open_controller),
                        step_size,
                        nullptr,
                        use_opencl,
                        false,
                        status.expander,
                        info.model != XDAQModel::Core,
                        (info.model == XDAQModel::Core ? 1 : 2)
                    );
                },
                info,
                status
            )
        );
    }

    insert_board(
        launch_panel,
        boardTable,
        get_demo_board(
            parent,
            [parent](
                std::function<AbstractRHXController *()> open_controller, StimStepSize step_size
            ) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                auto use_opencl = settings.value("useOpenCL", true).toBool();
                settings.endGroup();
                parent->emit launch(
                    std::move(open_controller),
                    step_size,
                    nullptr,
                    use_opencl,
                    false,
                    true,
                    false,
                    2
                );
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

    InsertBoard(this, launch_panel, boardTable);

    // Make table visible in full (for up to 5 rows... then allow a scroll bar to be used).
    boardTable->setIconSize(QSize(283, 100));
    boardTable->resizeColumnsToContents();
    boardTable->resizeRowsToContents();
    boardTable->setMinimumSize(calculateTableSize(boardTable, 4, 6));
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
        // settings.setValue("playbackPorts", playback_ports);
        settings.endGroup();
    });

    auto rescanDeviceButton = new QPushButton(tr("Rescan Devices"), this);
    rescanDeviceButton->setFixedWidth(rescanDeviceButton->sizeHint().width() + 10);
    connect(rescanDeviceButton, &QPushButton::clicked, this, [this, launch_panel, boardTable]() {
        while (auto widget = launch_panel->widget(0)) {
            launch_panel->removeWidget(widget);
        }
        boardTable->setRowCount(0);
        InsertBoard(this, launch_panel, boardTable);
        launch_panel->resize(launch_panel->currentWidget()->sizeHint());
        boardTable->resizeColumnsToContents();
        boardTable->resizeRowsToContents();
        boardTable->setMinimumSize(calculateTableSize(boardTable, 4, 6));
        this->resize(this->sizeHint());
    });

    auto mainLayout = new QVBoxLayout;
    auto boardsLayout = new QHBoxLayout;
    auto buttonsLayout = new QHBoxLayout;

    boardsLayout->addWidget(boardTable);
    boardsLayout->addWidget(launch_panel);
    buttonsLayout->addWidget(advancedButton, 0, Qt::AlignLeft);
    buttonsLayout->addWidget(rescanDeviceButton, 1, Qt::AlignLeft);

    mainLayout->addLayout(boardsLayout);
    // mainLayout->addWidget(create_default_settings_file_checkbox(parent));
    // mainLayout->addWidget(create_default_sample_rate_checkbox(parent));
    mainLayout->addLayout(buttonsLayout);

    setWindowTitle("Select XDAQ");
    setLayout(mainLayout);
}