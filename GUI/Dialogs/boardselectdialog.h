//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.3.2
//
//  Copyright (c) 2020-2024 Intan Technologies
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

#include <QDialog>
#include <QStackedWidget>
#include <QWidget>
#include <functional>

#include "abstractrhxcontroller.h"
#include "datafilereader.h"



class StackedWidget : public QStackedWidget
{
public:
    StackedWidget(QWidget *parent) : QStackedWidget(parent){};
    QSize sizeHint() const override { return currentWidget()->sizeHint(); }
};

class BoardSelectDialog : public QDialog
{
    Q_OBJECT
public:
    BoardSelectDialog(QWidget *parent);
signals:
    void launch(
        std::function<AbstractRHXController *()>
            open_controller,        // Physical or simulated controller
        StimStepSize step_size,     // Pass to Controller for AUX commands
        DataFileReader *data_file,  // For the seek events
        bool OpenCL, bool test_mode, bool with_expander, bool enable_vstim_control,
        int on_board_analog_io
    );
};
