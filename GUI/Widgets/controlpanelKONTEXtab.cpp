//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
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

#include "controlpanelKONTEXtab.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QVBoxLayout>
#include <iostream>
#include <vector>

#include "bandwidthdialog.h"
#include "rhxregisters.h"

using namespace std;

ControlPanelKONTEXTab::ControlPanelKONTEXTab(ControllerInterface *controllerInterface_,
                                             SystemState *state_, QWidget *parent)
    : QWidget(parent), state(state_), controllerInterface(controllerInterface_)
{
    auto RHDButton = new QRadioButton("RHD");
    auto RHSButton = new QRadioButton("RHS");
    RHDButton->setChecked(true);

    auto modebuttons = new QButtonGroup(this);
    modebuttons->addButton(RHDButton, 0);
    modebuttons->addButton(RHSButton, 1);
    connect(modebuttons, &QButtonGroup::idClicked, this, [this](int id) { this->rhd = id == 0; });

    auto *SPIBusLayout1 = new QHBoxLayout();
    SPIBusLayout1->addWidget(RHDButton);
    SPIBusLayout1->addWidget(RHSButton);
    SPIBusLayout1->addStretch(1);

    auto *SPIGroupBox = new QGroupBox(tr("SPI Bus"));
    SPIGroupBox->setLayout(SPIBusLayout1);


    auto V14Button = new QRadioButton("+14/-4V");
    auto V9Button = new QRadioButton("+/-9V");
    auto V4Button = new QRadioButton("+4/-14V");
    if(state->enableVStim){
        V9Button->setChecked(true);
    }else{
        V14Button->setEnabled(false);
        V9Button->setEnabled(false);
        V4Button->setEnabled(false);
    }

    auto VStimbuttons = new QButtonGroup(this);
    VStimbuttons->addButton(V9Button, 1);
    VStimbuttons->addButton(V14Button, 2);
    VStimbuttons->addButton(V4Button, 3);
    connect(VStimbuttons, &QButtonGroup::idClicked, this, [this](int id) { this->controllerInterface->setVStimBus(id); });

    QHBoxLayout *StimLayout1 = new QHBoxLayout();
    StimLayout1->addWidget(V9Button);
    StimLayout1->addWidget(V14Button);
    StimLayout1->addWidget(V4Button);
    StimLayout1->addStretch(1);

    QGroupBox *VStimGroupBox = new QGroupBox(tr("VStim Supply"));
    VStimGroupBox->setLayout(StimLayout1);

#ifdef DEBUG
    auto binOutputCheckBox = new QCheckBox(tr("Write Debug Bin Files"));
    connect(binOutputCheckBox, SIGNAL(clicked(bool)), this, SLOT(updateBinFlag(bool)));

    QHBoxLayout *wireOutLayout = new QHBoxLayout;
    auto WireOutButton = new QPushButton(tr("Get WireOut"));
    connect(WireOutButton, SIGNAL(clicked()), this, SLOT(getWireOut()));
    OutCMDLineEdit = new QLineEdit();
    OutCMDLineEdit->setText("0x28,");
    wireOutLayout->addWidget(OutCMDLineEdit);
    wireOutLayout->addWidget(WireOutButton);
    wireOutLayout->addStretch(1);

    QHBoxLayout *wireInLayout = new QHBoxLayout;
    auto WireInButton = new QPushButton(tr("Set WireIn"));
    connect(WireInButton, SIGNAL(clicked()), this, SLOT(setWireIn()));
    auto InCMDLineEdit = new QLineEdit();
    InCMDLineEdit->setText("0x1F,0x07");
    wireInLayout->addWidget(InCMDLineEdit);
    wireInLayout->addWidget(WireInButton);
    wireInLayout->addStretch(1);

    QHBoxLayout *TrigInLayout = new QHBoxLayout;
    auto TrigInButton = new QPushButton(tr("Set TrigIn"));
    connect(TrigInButton, SIGNAL(clicked()), this, SLOT(setTrigIn()));
    auto TrigCMDLineEdit = new QLineEdit();
    TrigCMDLineEdit->setText("0x40,0x0B");
    TrigInLayout->addWidget(TrigCMDLineEdit);
    TrigInLayout->addWidget(TrigInButton);
    TrigInLayout->addStretch(1);

    QHBoxLayout *OutDataLayout = new QHBoxLayout;
    OutDataLayout->addWidget(new QLabel("WireOut Data"));
    auto OutDataCMDLineEdit = new QLineEdit();
    OutDataCMDLineEdit->setText("");
    OutDataLayout->addWidget(OutDataCMDLineEdit);
    // OutDataLayout->addStretch(1);

    /*
    GetHWInfoButton = new QPushButton(tr("Send WireOut"));
    connect(GetHWInfoButton, SIGNAL(clicked()),
            this, SLOT(getKonteXHWConfiguration()));
    KonteXCMDLineEdit = new QLineEdit();
    KonteXCMDLineEdit->setText("0x15,0x11");
    SendHWControlButton = new QPushButton(tr("Send WireIn CMD"));
    connect(SendHWControlButton, SIGNAL(clicked()),
            this, SLOT(setKonteXHWConfiguration()));
    */

    QVBoxLayout *debugLayout = new QVBoxLayout;
    QVBoxLayout *debugLayout1 = new QVBoxLayout;
    debugLayout1->addWidget(binOutputCheckBox);


    debugLayout1->addWidget(new QLabel("Add,HexCMD"));
    debugLayout1->addLayout(wireInLayout);
    debugLayout1->addLayout(TrigInLayout);
    debugLayout1->addLayout(wireOutLayout);
    debugLayout1->addLayout(OutDataLayout);
    // debugLayout1->addWidget(KonteXCMDLineEdit);
    // debugLayout1->addWidget(GetHWInfoButton);
    // debugLayout1->addWidget(SendHWControlButton);
    debugLayout1->addStretch(1);


    QHBoxLayout *coaxLayout = new QHBoxLayout;
    auto enCoaxCheckBox = new QCheckBox(tr("Enable Coaxial Mode"));
    auto enCoaxButton = new QPushButton(tr("Set"));
    connect(enCoaxButton, SIGNAL(clicked()), this, SLOT(sendCoaxInit()));
    coaxLayout->addWidget(enCoaxCheckBox);
    coaxLayout->addWidget(enCoaxButton);
    coaxLayout->addStretch(1);

    debugLayout->addLayout(debugLayout1);
    debugLayout->addLayout(coaxLayout);
    QGroupBox *debugGroupBox = new QGroupBox(tr("KonteX Internal"));
    debugGroupBox->setLayout(debugLayout);


    auto SetTrig11Button = new QPushButton(tr("Set T11"));
    connect(SetTrig11Button, SIGNAL(clicked()), this, SLOT(setTrig11()));
#endif

    QVBoxLayout *KonteXLayout = new QVBoxLayout;
    KonteXLayout->addWidget(VStimGroupBox);
#ifdef DEBUG
    KonteXLayout->addWidget(SPIGroupBox);
    KonteXLayout->addWidget(SetTrig11Button);
    KonteXLayout->addWidget(debugGroupBox);
    // KonteXLayout->addLayout(coaxLayout);
#endif
    KonteXLayout->addStretch(1);

    setLayout(KonteXLayout);
}

void ControlPanelKONTEXTab::updateBinFlag(bool enabled)
{
    //!!controller->binOutput = enabled;
}

void ControlPanelKONTEXTab::setTrig11()
{
    //    setSPIBus();
    //    setVStim();
    int val = int(rhd) | (vstim + 1) << 1;

    //!!evalBoard->setTrig11(val);
}


void ControlPanelKONTEXTab::sendCoaxInit()
{
    //!!evalBoard->initializeCoax();
}

void ControlPanelKONTEXTab::setSPIBus()
{
    //!!evalBoard->setSPIBus(int(rhd));
}


void ControlPanelKONTEXTab::getKonteXHWConfiguration()
{
    cout << endl << "get KonteX Configuration" << endl;

    int KonteXConfig = 0;
    //!!evalBoard->getKonteXConfiguration();
    // int KonteXConfig = 255;
    QString response = QString::number(KonteXConfig, 16);
    KonteXCMDLineEdit->setText(response);
}

void ControlPanelKONTEXTab::setKonteXHWConfiguration()
{
    // evalBoard->binOutput = enabled;
    QStringList data = KonteXCMDLineEdit->text().split(',');
    // cout << endl << "set KonteX Configuration: " << endl << data[0].toStdString() << endl <<
    // data[1].toStdString() << endl; evalBoard->sendWireIn(data[0]., data[1]);
    bool ok;
    int cmd = data[0].toInt(&ok, 16);
    int val = data[1].toInt(&ok, 16);
    //!!evalBoard->sendWireIn(cmd,val);
}


void ControlPanelKONTEXTab::setWireIn()
{
    // evalBoard->binOutput = enabled;
    QStringList data = InCMDLineEdit->text().split(',');
    // cout << endl << "set KonteX Configuration: " << endl << data[0].toStdString() << endl <<
    // data[1].toStdString() << endl; evalBoard->sendWireIn(data[0]., data[1]);
    bool ok;
    int cmd = data[0].toInt(&ok, 16);
    int val = data[1].toInt(&ok, 16);
    //!! evalBoard->sendWireIn(cmd,val);
}

void ControlPanelKONTEXTab::setTrigIn()
{
    // evalBoard->binOutput = enabled;
    QStringList data = TrigCMDLineEdit->text().split(',');
    // cout << endl << "set KonteX Configuration: " << endl << data[0].toStdString() << endl <<
    // data[1].toStdString() << endl; evalBoard->sendWireIn(data[0]., data[1]);
    bool ok;
    int cmd = data[0].toInt(&ok, 16);
    int val = data[1].toInt(&ok, 16);

    //!!evalBoard->sendTrigIn(cmd,val);
}

void ControlPanelKONTEXTab::getWireOut()
{
    QStringList data = OutCMDLineEdit->text().split(',');
    // cout << endl << "set KonteX Configuration: " << endl << data[0].toStdString() << endl <<
    // data[1].toStdString() << endl; evalBoard->sendWireIn(data[0]., data[1]);
    bool ok;
    int cmd = data[0].toInt(&ok, 16);
    int cmdval = data[1].toInt(&ok, 16);
    int val = 0;
    //!!evalBoard->getWireOut(cmd);
    QString response = QString::number(val, 16);

    OutDataCMDLineEdit->setText(response);
}


void ControlPanelKONTEXTab::updateFromState() {}
