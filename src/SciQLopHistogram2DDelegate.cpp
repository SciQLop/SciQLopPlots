/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/

/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopHistogram2D.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>

SciQLopHistogram2D* SciQLopHistogram2DDelegate::histogram() const
{
    return as_type<SciQLopHistogram2D>(m_object);
}

SciQLopHistogram2DDelegate::SciQLopHistogram2DDelegate(SciQLopHistogram2D* object, QWidget* parent)
        : SciQLopColorMapBaseDelegate(object, parent)
{
    auto* binningBox = new QGroupBox("Binning", this);
    auto* binningLayout = new QFormLayout(binningBox);

    auto* keyBinsSpin = new QSpinBox(binningBox);
    keyBinsSpin->setRange(1, 100000);
    keyBinsSpin->setValue(object->key_bins());
    binningLayout->addRow("Key bins", keyBinsSpin);

    auto* valueBinsSpin = new QSpinBox(binningBox);
    valueBinsSpin->setRange(1, 100000);
    valueBinsSpin->setValue(object->value_bins());
    binningLayout->addRow("Value bins", valueBinsSpin);

    auto push_bins = [keyBinsSpin, valueBinsSpin, object]()
    {
        object->set_bins(keyBinsSpin->value(), valueBinsSpin->value());
    };
    connect(keyBinsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [push_bins](int) { push_bins(); });
    connect(valueBinsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [push_bins](int) { push_bins(); });

    m_layout->addRow(binningBox);

    auto* normCombo = new QComboBox(this);
    normCombo->addItem("None", 0);            // QCPHistogram2D::nNone
    normCombo->addItem("Per-column", 1);      // QCPHistogram2D::nColumn
    normCombo->setCurrentIndex(normCombo->findData(object->normalization()));
    m_layout->addRow("Normalization", normCombo);
    connect(normCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [normCombo, object](int)
            {
                object->set_normalization(normCombo->currentData().toInt());
            });

    // Reverse path
    connect(object, &SciQLopHistogram2D::bins_changed, this,
            [keyBinsSpin, valueBinsSpin](int kb, int vb)
            {
                QSignalBlocker bk(keyBinsSpin);
                QSignalBlocker bv(valueBinsSpin);
                keyBinsSpin->setValue(kb);
                valueBinsSpin->setValue(vb);
            });
    connect(object, &SciQLopHistogram2D::normalization_changed, this,
            [normCombo](int n)
            {
                QSignalBlocker b(normCombo);
                normCombo->setCurrentIndex(normCombo->findData(n));
            });

    append_inspector_extensions();
}
