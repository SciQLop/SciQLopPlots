/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTimelineDelegate.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSpinBox>

namespace
{
// Labels in enum order: a combo index or a box position is the enum value.
const QStringList kStyles { "wave", "bars" };
const QStringList kStacks { "none", "time", "category" };
const QStringList kEditModes { "move", "resize", "change_lane", "create", "delete" };
}

SciQLopTimelineDelegate::SciQLopTimelineDelegate(SciQLopTimeline* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    add_style_row(object);
    add_stacking_rows(object);
    add_lane_height_row(object);
    add_editing_rows(object);
    add_snap_rows(object);
}

void SciQLopTimelineDelegate::add_style_row(SciQLopTimeline* tl)
{
    auto* style = new QComboBox;
    style->setObjectName("style");
    style->addItems(kStyles);
    style->setCurrentIndex(static_cast<int>(tl->style()));
    fit_combo_to_content(style);
    m_layout->addRow("Style", style);
    connect(style, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                if (auto* t = timeline())
                    t->set_style(static_cast<TimelineStyle>(index));
            });
    connect(tl, &SciQLopTimeline::style_changed, style,
            [style](TimelineStyle s)
            {
                QSignalBlocker b(style);
                style->setCurrentIndex(static_cast<int>(s));
            });
}

void SciQLopTimelineDelegate::add_stacking_rows(SciQLopTimeline* tl)
{
    auto* stack = new QComboBox;
    stack->setObjectName("stack");
    stack->addItems(kStacks);
    stack->setCurrentIndex(static_cast<int>(tl->stack()));
    fit_combo_to_content(stack);
    m_layout->addRow("Stack overlaps", stack);
    connect(stack, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                if (auto* t = timeline())
                    t->set_stack(static_cast<TimelineStack>(index));
            });
    connect(tl, &SciQLopTimeline::stack_changed, stack,
            [stack](TimelineStack s)
            {
                QSignalBlocker b(stack);
                stack->setCurrentIndex(static_cast<int>(s));
            });

    auto* forbid = new QCheckBox("Forbid overlaps");
    forbid->setObjectName("forbid_overlap");
    forbid->setChecked(tl->forbid_overlap());
    m_layout->addRow("", forbid);
    connect(forbid, &QCheckBox::toggled, this,
            [this](bool on)
            {
                if (auto* t = timeline())
                    t->set_forbid_overlap(on);
            });
    connect(tl, &SciQLopTimeline::forbid_overlap_changed, forbid,
            [forbid](bool on)
            {
                QSignalBlocker b(forbid);
                forbid->setChecked(on);
            });
}

void SciQLopTimelineDelegate::add_lane_height_row(SciQLopTimeline* tl)
{
    auto* height = new QSpinBox;
    height->setObjectName("lane_height");
    height->setRange(8, 200);
    height->setSuffix(" px");
    height->setValue(tl->lane_height());
    m_layout->addRow("Lane height", height);
    connect(height, &QSpinBox::valueChanged, this,
            [this](int px)
            {
                if (auto* t = timeline())
                    t->set_lane_height(px);
            });
}

void SciQLopTimelineDelegate::add_editing_rows(SciQLopTimeline* tl)
{
    auto* editable = new QCheckBox("Editable");
    editable->setObjectName("editable");
    editable->setChecked(tl->editable());
    m_layout->addRow("", editable);
    connect(editable, &QCheckBox::toggled, this,
            [this](bool on)
            {
                if (auto* t = timeline())
                    t->set_editable(on);
            });
    connect(tl, &SciQLopTimeline::editable_changed, editable,
            [editable](bool on)
            {
                QSignalBlocker b(editable);
                editable->setChecked(on);
            });

    const auto enabled = tl->edit_modes();
    for (int i = 0; i < kEditModes.size(); ++i)
    {
        const auto mode = static_cast<TimelineEditMode>(i);
        auto* box = new QCheckBox(QString(kEditModes[i]).replace('_', ' '));
        box->setObjectName("edit_mode_" + kEditModes[i]);
        box->setChecked(enabled.contains(mode));
        m_layout->addRow(i == 0 ? "Edit modes" : "", box);
        connect(box, &QCheckBox::toggled, this,
                [this, mode](bool on)
                {
                    auto* t = timeline();
                    if (!t)
                        return;
                    auto modes = t->edit_modes();
                    modes.removeAll(mode);
                    if (on)
                        modes.append(mode);
                    t->set_edit_modes(modes);
                });
        connect(tl, &SciQLopTimeline::edit_modes_changed, box,
                [box, mode](const QList<TimelineEditMode>& modes)
                {
                    QSignalBlocker b(box);
                    box->setChecked(modes.contains(mode));
                });
    }
}

void SciQLopTimelineDelegate::add_snap_rows(SciQLopTimeline* tl)
{
    m_snap = new QComboBox;
    m_snap->setObjectName("snap");
    m_snap->addItems({ "none", "edges", "step" });
    if (tl->snap_mode() == "times")
        m_snap->addItem("times"); // set from code (snap_to=[...]); kept as is here
    m_snap->setCurrentText(tl->snap_mode());
    fit_combo_to_content(m_snap);
    m_layout->addRow("Snap", m_snap);

    m_snapStep = new QDoubleSpinBox;
    m_snapStep->setObjectName("snap_step");
    m_snapStep->setRange(0.001, 1e9);
    m_snapStep->setDecimals(3);
    m_snapStep->setSuffix(" s");
    m_snapStep->setValue(tl->snap_step() > 0 ? tl->snap_step() : 60);
    m_snapStep->setEnabled(m_snap->currentText() == "step");
    m_layout->addRow("Snap step", m_snapStep);

    connect(m_snap, &QComboBox::currentTextChanged, this, [this] { apply_snap(); });
    connect(m_snapStep, &QDoubleSpinBox::valueChanged, this, [this] { apply_snap(); });
}

void SciQLopTimelineDelegate::apply_snap()
{
    auto* t = timeline();
    if (!t)
        return;
    const QString mode = m_snap->currentText();
    m_snapStep->setEnabled(mode == "step");
    if (mode == "edges")
        t->set_snap_edges();
    else if (mode == "step")
        t->set_snap_step(m_snapStep->value());
    else if (mode == "none")
        t->clear_snap();
}
