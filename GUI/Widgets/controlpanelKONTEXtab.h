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

#pragma once

#include <QLineEdit>
#include <QWidget>

#include "controllerinterface.h"
#include "systemstate.h"
#include "viewfilterswindow.h"

class ControlPanelKONTEXTab : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanelKONTEXTab(ControllerInterface *controllerInterface_, SystemState *state_,
                                   QWidget *parent = nullptr);
    ~ControlPanelKONTEXTab() = default;

    void updateFromState();

private slots:

    void updateBinFlag(bool enabled);
    void getKonteXHWConfiguration();  //
    void setKonteXHWConfiguration();  //
    void setSPIBus();
    void setTrig11();
    void sendCoaxInit();
    void setTrigIn();
    void setWireIn();
    void getWireOut();

private:
    SystemState *state;
    ControllerInterface *controllerInterface;
    bool rhd = true;
    int vstim = 1;
    QLineEdit *KonteXCMDLineEdit;
    QLineEdit *OutCMDLineEdit;
    QLineEdit *TrigCMDLineEdit;
    QLineEdit *InCMDLineEdit;
    QLineEdit *OutDataCMDLineEdit;
};

class ControlPanelCloseLoop : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanelCloseLoop(ControllerInterface *controllerInterface_, SystemState *state_,
                                   QWidget *parent = nullptr);

private:
    SystemState *state;
    ControllerInterface *controllerInterface;
};