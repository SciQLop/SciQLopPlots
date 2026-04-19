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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

SciQLopWaterfallDelegate::SciQLopWaterfallDelegate(SciQLopWaterfallGraph* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("Uniform", QVariant::fromValue(WaterfallOffsetMode::Uniform));
    m_modeCombo->addItem("Custom", QVariant::fromValue(WaterfallOffsetMode::Custom));
    m_modeCombo->setCurrentIndex(object->offset_mode() == WaterfallOffsetMode::Uniform ? 0 : 1);
    m_layout->addRow("Offset mode", m_modeCombo);

    m_spacingSpin = new QDoubleSpinBox;
    m_spacingSpin->setRange(-1e9, 1e9);
    m_spacingSpin->setDecimals(4);
    m_spacingSpin->setValue(object->uniform_spacing());
    m_layout->addRow("Uniform spacing", m_spacingSpin);

    m_offsetsScroll = new QScrollArea;
    m_offsetsScroll->setWidgetResizable(true);
    auto* offsetsHost = new QWidget;
    m_offsetsLayout = new QVBoxLayout(offsetsHost);
    m_offsetsScroll->setWidget(offsetsHost);
    m_offsetsScroll->setMaximumHeight(120);
    m_layout->addRow("Offsets", m_offsetsScroll);
    rebuild_offsets_editor();

    m_normalizeCheck = new QCheckBox("Normalize per trace");
    m_normalizeCheck->setChecked(object->normalize());
    m_layout->addRow("", m_normalizeCheck);

    m_gainSpin = new QDoubleSpinBox;
    m_gainSpin->setRange(-1e6, 1e6);
    m_gainSpin->setDecimals(3);
    m_gainSpin->setValue(object->gain());
    m_layout->addRow("Gain", m_gainSpin);

    refresh_mode_enabled_state();

    // Widgets -> model.
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int)
            {
                auto* w = waterfall();
                if (!w)
                    return;
                w->set_offset_mode(m_modeCombo->currentData().value<WaterfallOffsetMode>());
                refresh_mode_enabled_state();
            });
    connect(m_spacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v)
            {
                if (auto* w = waterfall())
                    w->set_uniform_spacing(v);
            });
    connect(m_normalizeCheck, &QCheckBox::toggled, this,
            [this](bool v)
            {
                if (auto* w = waterfall())
                    w->set_normalize(v);
            });
    connect(m_gainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v)
            {
                if (auto* w = waterfall())
                    w->set_gain(v);
            });

    // Model -> widgets.
    connect(object, &SciQLopWaterfallGraph::offset_mode_changed, this,
            [this](WaterfallOffsetMode m)
            {
                m_modeCombo->setCurrentIndex(m == WaterfallOffsetMode::Uniform ? 0 : 1);
                refresh_mode_enabled_state();
            });
    connect(object, &SciQLopWaterfallGraph::uniform_spacing_changed, this,
            [this](double v) { m_spacingSpin->setValue(v); });
    connect(object, &SciQLopWaterfallGraph::offsets_changed, this,
            [this](const QVector<double>&) { rebuild_offsets_editor(); });
    connect(object, &SciQLopWaterfallGraph::normalize_changed, this,
            [this](bool v) { m_normalizeCheck->setChecked(v); });
    connect(object, &SciQLopWaterfallGraph::gain_changed, this,
            [this](double v) { m_gainSpin->setValue(v); });
    connect(object, QOverload<>::of(&SciQLopGraphInterface::data_changed), this,
            [this]() { rebuild_offsets_editor(); });

    append_inspector_extensions();
}

void SciQLopWaterfallDelegate::rebuild_offsets_editor()
{
    auto* w = waterfall();
    if (!w)
        return;
    const auto current = w->offsets();
    const int n = static_cast<int>(w->line_count());
    QVector<double> padded = current;
    while (padded.size() < n)
        padded.append(0.0);
    padded.resize(n);

    // Same-count fast path: update values in place. Preserves focus during
    // streaming and breaks the widget→model→signal→widget rebuild cycle.
    if (m_offsetsSpins.size() == n)
    {
        for (int i = 0; i < n; ++i)
        {
            QSignalBlocker blocker(m_offsetsSpins[i]);
            m_offsetsSpins[i]->setValue(padded[i]);
        }
        return;
    }

    for (auto* s : m_offsetsSpins)
        s->deleteLater();
    m_offsetsSpins.clear();

    for (int i = 0; i < n; ++i)
    {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(-1e9, 1e9);
        spin->setDecimals(4);
        spin->setValue(padded[i]);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double)
                {
                    auto* w = waterfall();
                    if (!w)
                        return;
                    if (m_offsetsSpins.size() != static_cast<int>(w->line_count()))
                        return;
                    QVector<double> v;
                    v.reserve(m_offsetsSpins.size());
                    for (auto* s : m_offsetsSpins)
                        v.append(s->value());
                    w->set_offsets(v);
                });
        m_offsetsLayout->addWidget(spin);
        m_offsetsSpins.append(spin);
    }
}

void SciQLopWaterfallDelegate::refresh_mode_enabled_state()
{
    auto* w = waterfall();
    if (!w)
        return;
    const bool uniform = w->offset_mode() == WaterfallOffsetMode::Uniform;
    m_spacingSpin->setEnabled(uniform);
    m_offsetsScroll->setEnabled(!uniform);
}
