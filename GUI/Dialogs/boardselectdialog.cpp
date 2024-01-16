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

#include <QApplication>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QtGlobal>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>

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

class StackedWidget : public QStackedWidget
{
public:
    QSize sizeHint() const override { return currentWidget()->sizeHint(); }
};

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
    int width = boardTable->verticalHeader()->width() + 8;
    for (int column = 0; column < boardTable->columnCount(); column++) {
        width += boardTable->columnWidth(column) + 4;
    }

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
            [launch_properties, port](int state) {
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
                launch_properties->at("playbackPorts") = ports;
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

auto get_xdaq_board(QWidget *parent, auto launch, const XDAQInfo &info)
{
    auto app_icon = new QTableWidgetItem(
        getIcon(info.model, parent->style(), info.model == XDAQModel::Unknown ? 40 : 80),
        QString::fromStdString(info.id)
    );
    auto layout = new QVBoxLayout;
    layout->addWidget(new QLabel(QString::fromStdString(info.plugin)));
    // Report if an io expander is connected.
    if (info.model == XDAQModel::Unknown) {
        layout->addWidget(new QLabel(parent->tr("N/A")));
    } else if (info.expander) {
        auto expander_layout = new QHBoxLayout;
        auto icon = new QLabel();
        icon->setPixmap(parent->style()->standardIcon(QStyle::SP_DialogYesButton).pixmap(20));
        expander_layout->addWidget(icon);
        expander_layout->addWidget(new QLabel(parent->tr("I/O Expander Connected")));
        layout->addLayout(expander_layout);
    } else {
        auto expander_layout = new QHBoxLayout;
        auto icon = new QLabel();
        icon->setPixmap(parent->style()->standardIcon(QStyle::SP_DialogNoButton).pixmap(20));
        expander_layout->addWidget(icon);
        expander_layout->addWidget(new QLabel(parent->tr("No I/O Expander Connected")));
        layout->addLayout(expander_layout);
    }

    // Report the serial number of this board.
    auto serial_layout = new QHBoxLayout;
    serial_layout->addWidget(new QLabel(parent->tr("Serial Number")));
    serial_layout->addWidget(new QLabel(QString::fromStdString(info.serial)));
    layout->addLayout(serial_layout);
    auto device_widget = new QWidget();
    device_widget->setLayout(layout);
    device_widget->setDisabled(info.model == XDAQModel::Unknown);

    std::shared_ptr<json> launch_properties =
        std::shared_ptr<json>(new json{{"sample_rate", SampleRate30000Hz}, {"stim_step_size", 5}});
    auto launch_button_rhd = new QPushButton(parent->tr("Record (X3R/X6R)"));
    QObject::connect(launch_button_rhd, &QPushButton::clicked, [launch, info, launch_properties]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        launch(
            new RHXController(
                ControllerType::ControllerRecordUSB3,
                launch_properties->at("sample_rate"),
                info.get_device(info.device_config)
            ),
            launch_properties->at("stim_step_size")
        );
        settings.endGroup();
    });
    auto launch_button_rhs = new QPushButton(parent->tr("Stim-Record (X3SR)"));
    QObject::connect(launch_button_rhs, &QPushButton::clicked, [launch, info, launch_properties]() {
        QSettings settings;
        settings.beginGroup("XDAQ");
        launch(
            new RHXController(
                ControllerType::ControllerStimRecord,
                launch_properties->at("sample_rate"),
                info.get_device(info.device_config)
            ),
            launch_properties->at("stim_step_size")
        );
        settings.endGroup();
    });
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

// Create a dialog window for user to select which board's software to initialize.
BoardSelectDialog::BoardSelectDialog(QWidget *parent, const std::vector<XDAQInfo> &xdaq_infos)
    : QDialog(parent)
{
    auto launch_panel = new StackedWidget();
    auto boardTable = new QTableWidget(0, 2, nullptr);
    // Set up header.
    boardTable->setHorizontalHeaderLabels({tr("App"), tr("Info")});
    boardTable->horizontalHeader()->setSectionsClickable(false);
    boardTable->verticalHeader()->setSectionsClickable(false);
    boardTable->setFocusPolicy(Qt::ClickFocus);

    std::vector<std::shared_ptr<json>> board_launch_properties;

    auto insert_board = [boardTable, launch_panel, &board_launch_properties](auto &&board) {
        auto [app_icon, device_widget, launch_button_widget, launch_properties] = board;
        board_launch_properties.push_back(launch_properties);
        auto row = boardTable->rowCount();
        boardTable->insertRow(row);
        boardTable->setItem(row, 0, app_icon);
        boardTable->setCellWidget(row, 1, device_widget);
        launch_button_widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        launch_panel->addWidget(launch_button_widget);
    };

    // Insert playback board.
    insert_board(get_playback_board(
        this,
        [this](AbstractRHXController *controller, DataFileReader *data_file) {
            QSettings settings;
            settings.beginGroup("XDAQ");
            auto use_opencl = settings.value("useOpenCL", true).toBool();
            settings.endGroup();
            this->emit launch(controller, data_file->stimStepSize(), data_file, use_opencl, false);
        }
    ));

    for (const auto &info : xdaq_infos) {
        insert_board(get_xdaq_board(
            this,
            [this](AbstractRHXController *controller, StimStepSize step_size) {
                QSettings settings;
                settings.beginGroup("XDAQ");
                auto use_opencl = settings.value("useOpenCL", true).toBool();
                settings.endGroup();
                this->emit launch(controller, step_size, nullptr, use_opencl, false);
            },
            info
        ));
    }

    insert_board(get_demo_board(
        this,
        [this](AbstractRHXController *controller, StimStepSize step_size) {
            QSettings settings;
            settings.beginGroup("XDAQ");
            auto use_opencl = settings.value("useOpenCL", true).toBool();
            settings.endGroup();
            this->emit launch(controller, step_size, nullptr, use_opencl, false);
        }
    ));


    // Make table visible in full (for up to 5 rows... then allow a scroll bar to be used).
    boardTable->setIconSize(QSize(283, 100));
    boardTable->resizeColumnsToContents();
    boardTable->resizeRowsToContents();
    boardTable->setMinimumSize(calculateTableSize(boardTable));
    boardTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    boardTable->setSelectionMode(QAbstractItemView::SingleSelection);

    launch_panel->addWidget(new QLabel(tr("Select a board to launch")));
    launch_panel->setCurrentIndex(launch_panel->count() - 1);
    connect(boardTable, &QTableWidget::currentCellChanged, [boardTable, launch_panel, this]() {
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
        bool use_opencl = true;
        std::uint8_t playback_ports = 255;
        AdvancedStartupDialog advancedStartupDialog(use_opencl, playback_ports, false, this);
        advancedStartupDialog.exec();
        settings.setValue("useOpenCL", use_opencl);
        settings.setValue("playbackPorts", playback_ports);
        settings.endGroup();
    });

    auto mainLayout = new QVBoxLayout;
    auto boardsLayout = new QHBoxLayout;
    boardsLayout->addWidget(boardTable);
    boardsLayout->addWidget(launch_panel);
    mainLayout->addLayout(boardsLayout);
    mainLayout->addWidget(create_default_settings_file_checkbox(nullptr));
    mainLayout->addWidget(create_default_sample_rate_checkbox(nullptr));
    mainLayout->addWidget(advancedButton);

    setWindowTitle("Select XDAQ");
    setLayout(mainLayout);

    // auto splash = new QSplashScreen(QPixmap(":images/RHX_splash.png"));
    // auto splashMessage = "Copyright " + CopyrightSymbol + " " + ApplicationCopyrightYear +
    //                      " Intan Technologies.  RHX version " + SoftwareVersion +
    //                      ".  Opening XDAQ ...";
    // auto splashMessageAlign = Qt::AlignCenter | Qt::AlignBottom;
    // auto splashMessageColor = Qt::white;
}