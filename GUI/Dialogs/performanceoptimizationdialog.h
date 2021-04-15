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

#ifndef PERFORMANCEOPTIMIZATIONDIALOG_H
#define PERFORMANCEOPTIMIZATIONDIALOG_H

#include <QDialog>

#include "rhxglobals.h"
#include "systemstate.h"


class QGroupBox;
class QLabel;
class QComboBox;
class QDialogButtonBox;

class PerformanceOptimizationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PerformanceOptimizationDialog(SystemState* state_, QWidget *parent = nullptr);

    QComboBox *XPUSelectionComboBox;

signals:
    void usedXPUIndexChanged(int index);

private:
    void initialize();

    SystemState* state;
    QDialogButtonBox *buttonBox;
};

#endif // PERFORMANCEOPTIMIZATIONDIALOG_H
