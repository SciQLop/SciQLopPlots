/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#pragma once

#include "SciQLopPlotItem.hpp"
#include "constants.hpp"
#include <QBrush>
#include <QColor>
#include <QRgb>
#include <qcustomplot.h>

class VerticalSpanBorder : public SciQLopPlotItem<QCPItemStraightLine>,
                           public SciQlopItemWithToolTip
{
    Q_OBJECT

public:
#ifndef BINDINGS_H
    Q_SIGNAL void moved(double new_position);
#endif
    inline VerticalSpanBorder(QCustomPlot* plot, double x, bool do_not_replot = false)
            : SciQLopPlotItem { plot }
    {

        this->point1->setTypeX(QCPItemPosition::ptPlotCoords);
        this->point1->setTypeY(QCPItemPosition::ptAxisRectRatio);
        this->point2->setTypeX(QCPItemPosition::ptPlotCoords);
        this->point2->setTypeY(QCPItemPosition::ptAxisRectRatio);
        this->setPen(QPen { QBrush { QColor(0, 255, 255, 255), Qt::SolidPattern }, 3 });
        this->setSelectable(true);
        this->setMovable(true);
        this->setLayer(Constants::LayersNames::SpansBorders);
        this->set_position(x);
        if (!do_not_replot)
            this->replot();
    }

    inline void set_color(const QColor& color)
    {
        this->setPen(QPen { QBrush { color, Qt::SolidPattern }, 3 });
    }

    double position()
    {
        if (this->point1->coords().x() == this->point2->coords().x())
            return this->point1->coords().x();
        else
            return std::nan("");
    }

    inline void set_position(double x)
    {
        if (this->position() != x)
        {
            this->point1->setCoords(x, 0);
            this->point2->setCoords(x, 1);
            this->replot();
        }
    }

    void move(double dx, double dy) override;
};


class VerticalSpan : public SciQLopPlotItem<QCPItemRect>,
                     public SciQlopItemWithToolTip,
                     public SciQLopItemWithKeyInteraction
{
    Q_OBJECT
    inline void set_auto_extend_vertically()
    {
        this->topLeft->setTypeX(QCPItemPosition::ptPlotCoords);
        this->bottomRight->setTypeX(QCPItemPosition::ptPlotCoords);

        this->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
        this->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    }

    bool _lower_border_selected = false;
    bool _upper_border_selected = false;

    VerticalSpanBorder* _border1;
    VerticalSpanBorder* _border2;

    inline VerticalSpanBorder* _lower_border() const
    {
        if (this->_border1->position() <= this->_border2->position())
        {
            return this->_border1;
        }
        return this->_border2;
    }
    inline VerticalSpanBorder* _upper_border() const
    {
        if (this->_border2->position() >= this->_border1->position())
        {
            return this->_border2;
        }
        return this->_border1;
    }

    inline void set_left_pos(double pos)
    {
        this->topLeft->setCoords({ pos, 0. });
        this->_lower_border()->set_position(pos);
        if (_lower_border_selected == true and _lower_border()->selected() == false)
        {
            _upper_border()->setSelected(false);
            _lower_border()->setSelected(true);
        }
    }

    inline void set_right_pos(double pos)
    {
        this->bottomRight->setCoords({ pos, 1. });
        this->_upper_border()->set_position(pos);
        if (_upper_border_selected == true and _upper_border()->selected() == false)
        {
            _lower_border()->setSelected(false);
            _upper_border()->setSelected(true);
        }
    }

    void border1_selection_changed(bool select);

    void border2_selection_changed(bool select);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(QCPRange new_time_range);
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);
#endif

    VerticalSpan(QCustomPlot* plot, QCPRange horizontal_range, bool do_not_replot = false,
        bool immediate_replot = false);

    inline void setMovable(bool movable) noexcept final
    {
        SciQLopPlotItem::setMovable(movable);
        this->_border1->setMovable(movable);
        this->_border2->setMovable(movable);
    }

    ~VerticalSpan()
    {
        this->parentPlot()->removeItem(this->_border1);
        this->parentPlot()->removeItem(this->_border2);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (this->selected())
        {
            if (event->key() == Qt::Key_Delete)
            {
                this->parentPlot()->removeItem(this);
                this->parentPlot()->removeItem(this->_border1);
                this->parentPlot()->removeItem(this->_border2);
                this->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            }
        }
    }

    inline void set_visible(bool visible)
    {
        this->setVisible(visible);
        this->_border1->setVisible(visible);
        this->_border2->setVisible(visible);
    }

    void set_range(const QCPRange horizontal_range);

    [[nodiscard]] inline QCPRange range() const noexcept
    {
        return QCPRange { this->_lower_border()->position(), this->_upper_border()->position() };
    }

    void move(double dx, double dy) override;

    double selectTest(
        const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

    inline void set_borders_tool_tip(const QString& tool_tip)
    {
        this->_border1->setToolTip(tool_tip);
        this->_border2->setToolTip(tool_tip);
    }

    inline void set_borders_color(const QColor& color)
    {
        this->_border1->set_color(color);
        this->_border2->set_color(color);
    }

    [[nodiscard]] inline QColor borders_color() const noexcept
    {
        return this->_border1->pen().color();
    }

    void select_lower_border(bool selected);

    void select_upper_border(bool selected);

    inline void set_color(const QColor& color)
    {
        this->setBrush(QBrush { color, Qt::SolidPattern });
        this->setSelectedBrush(QBrush {
            QColor(255 - color.red(), 255 - color.green(), 255 - color.blue(), color.alpha()),
            Qt::SolidPattern });
        this->setPen(QPen { Qt::NoPen });
        this->setSelectedPen(QPen { Qt::NoPen });
    }
    [[nodiscard]] inline QColor color() const noexcept { return this->brush().color(); }

    inline void replot(bool immediate = false) final
    {
        // Only replot immediately the second border, the first border will be reploted with the
        // second.
        this->_border1->replot(false);
        this->_border2->replot(immediate);
        SciQLopPlotItem::replot(immediate);
    }
};


class SciQLopVerticalSpan : public QObject
{
    Q_OBJECT
    VerticalSpan* _impl;

    friend class MultiPlotsVerticalSpan;

protected:
    inline void select_lower_border(bool selected) { _impl->select_lower_border(selected); }
    inline void select_upper_border(bool selected) { _impl->select_upper_border(selected); }
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(QCPRange new_time_range);
    Q_SIGNAL void selectionChanged(bool);
#endif

    SciQLopVerticalSpan(QCustomPlot* plot, QCPRange horizontal_range, bool do_not_replot = false);
    ~SciQLopVerticalSpan()
    {
        if (this->_impl)
        {
            auto plot = this->_impl->parentPlot();
            if (plot->removeItem(this->_impl))
            {
                plot->replot(QCustomPlot::rpQueuedReplot);
            }
        }
    }


    inline void set_visible(bool visible) { _impl->set_visible(visible); }
    inline bool visible() const noexcept { return _impl->visible(); }

    inline void set_range(const QCPRange horizontal_range)
    {
        this->_impl->set_range(horizontal_range);
    }
    [[nodiscard]] inline QCPRange range() const noexcept { return this->_impl->range(); }

    inline void set_color(const QColor& color) { this->_impl->set_color(color); }
    [[nodiscard]] inline QColor color() const { return this->_impl->color(); }

    inline void set_borders_color(const QColor& color) { this->_impl->set_borders_color(color); }
    [[nodiscard]] inline QColor borders_color() const noexcept
    {
        return this->_impl->borders_color();
    }

    inline void set_selected(bool selected) { this->_impl->setSelected(selected); }
    [[nodiscard]] inline bool selected() const noexcept { return this->_impl->selected(); }

    inline void set_read_only(bool read_only) { this->_impl->setMovable(!read_only); }
    [[nodiscard]] inline bool read_only() const noexcept { return !this->_impl->movable(); }

    inline void set_tool_tip(const QString& tool_tip)
    {
        this->_impl->setToolTip(tool_tip);
        this->_impl->set_borders_tool_tip(tool_tip);
    }
    [[nodiscard]] inline QString tool_tip() const noexcept { return this->_impl->tooltip(); }

    inline void replot() { this->_impl->replot(); }
};

class MultiPlotsVerticalSpan : public QObject
{
    Q_OBJECT
    QList<SciQLopVerticalSpan*> _spans;
    QList<QCustomPlot*> _plots;
    QCPRange _horizontal_range;
    bool _selected = false;
    bool _lower_border_selected = false;
    bool _upper_border_selected = false;
    bool _visible = true;
    bool _read_only = false;
    QColor _color;
    QString _tool_tip;

    void select_lower_border(bool selected);
    void select_upper_border(bool selected);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(QCPRange new_time_range);
    Q_SIGNAL void selection_changed(bool);
#endif

    MultiPlotsVerticalSpan(QList<QCustomPlot*> plots, QCPRange horizontal_range, QColor color,
        bool read_only = false, bool visible = true, const QString& tool_tip = "",
        QObject* parent = nullptr)
    {
        _horizontal_range = horizontal_range;
        _color = color;
        _visible = visible;
        _read_only = read_only;
        _tool_tip = tool_tip;
        update_plot_list(plots);
    }

    ~MultiPlotsVerticalSpan()
    {
        for (auto span : _spans)
        {
            delete span;
        }
    }

    void update_plot_list(QList<QCustomPlot*> new_plots);

    void set_selected(bool selected);

    [[nodiscard]] inline bool is_selected() const noexcept { return _selected; }

    inline void set_color(const QColor& color)
    {
        if (_color != color)
        {
            for (auto span : _spans)
            {
                span->set_color(color);
            }
            _color = color;
        }
    }

    inline QColor get_color() const { return _color; }

    void set_range(const QCPRange horizontal_range);

    [[nodiscard]] inline QCPRange get_range() const noexcept { return _horizontal_range; }

    inline void set_visible(bool visible)
    {
        if (_visible != visible)
        {
            for (auto span : _spans)
            {
                span->set_visible(visible);
            }
            _visible = visible;
        }
    }

    [[nodiscard]] inline bool is_visible() const noexcept { return _visible; }

    inline void set_tool_tip(const QString& tool_tip)
    {
        if (_tool_tip != tool_tip)
        {
            for (auto span : _spans)
            {
                span->set_tool_tip(tool_tip);
            }
            _tool_tip = tool_tip;
        }
    }

    [[nodiscard]] inline QString get_tool_tip() const noexcept { return _tool_tip; }


    inline void set_read_only(bool read_only)
    {
        if (_read_only != read_only)
        {
            for (auto span : _spans)
            {
                span->set_read_only(read_only);
            }
            _read_only = read_only;
        }
    }

    [[nodiscard]] inline bool is_read_only() const noexcept { return _read_only; }


    inline void show()
    {
        for (auto span : _spans)
        {
            span->set_visible(true);
        }
    }


    inline void hide()
    {
        for (auto span : _spans)
        {
            span->set_visible(false);
        }
    }
};
