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

#include "../MultiPlots/SciQLopMultiPlotObject.hpp"
#include "../constants.hpp"
#include "SciQLopPlotItem.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
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

    ~VerticalSpanBorder() = default;

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

    inline QCursor cursor(QMouseEvent*) const noexcept override
    {
        if (movable())
            return Qt::SizeHorCursor;
        return Qt::ArrowCursor;
    }
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

    QPointer<VerticalSpanBorder> _border1;
    QPointer<VerticalSpanBorder> _border2;

    inline VerticalSpanBorder* _lower_border() const
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            if (this->_border1->position() <= this->_border2->position())
            {
                return this->_border1.data();
            }
            return this->_border2.data();
        }
        return nullptr;
    }

    inline VerticalSpanBorder* _upper_border() const
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            if (this->_border2->position() >= this->_border1->position())
            {
                return this->_border2.data();
            }
            return this->_border1.data();
        }
        return nullptr;
    }

    inline void set_left_pos(double pos)
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            this->topLeft->setCoords({ pos, 0. });
            this->_lower_border()->set_position(pos);
            if (_lower_border_selected == true and _lower_border()->selected() == false)
            {
                _upper_border()->setSelected(false);
                _lower_border()->setSelected(true);
            }
        }
    }

    inline void set_right_pos(double pos)
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            this->bottomRight->setCoords({ pos, 1. });
            _upper_border()->set_position(pos);
            if (_upper_border_selected == true and _upper_border()->selected() == false)
            {
                _lower_border()->setSelected(false);
                _upper_border()->setSelected(true);
            }
        }
    }

    void border1_selection_changed(bool select);

    void border2_selection_changed(bool select);

public:
#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);
    Q_SIGNAL void delete_requested();
#endif

    VerticalSpan(QCustomPlot* plot, SciQLopPlotRange horizontal_range, bool do_not_replot = false,
                 bool immediate_replot = false);

    inline void setMovable(bool movable) noexcept final
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            SciQLopPlotItem::setMovable(movable);
            this->_border1->setMovable(movable);
            this->_border2->setMovable(movable);
        }
    }

    ~VerticalSpan()
    {
        if (parentPlot()->hasItem(this->_border1))
        {
            parentPlot()->removeItem(this->_border1);
            parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
        if (parentPlot()->hasItem(this->_border2))
        {
            parentPlot()->removeItem(this->_border2);
            parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
    }

    inline QCursor cursor(QMouseEvent*) const noexcept override
    {
        if (movable())
            return Qt::SizeAllCursor;
        return Qt::ArrowCursor;
    }

    void keyPressEvent(QKeyEvent* event) override;

    inline void set_visible(bool visible)
    {
        this->setVisible(visible);
        if (!_border1.isNull() && !_border2.isNull())
        {
            this->_border1->setVisible(visible);
            this->_border2->setVisible(visible);
        }
    }

    void set_range(const SciQLopPlotRange horizontal_range);

    [[nodiscard]] inline SciQLopPlotRange range() const noexcept
    {
        return SciQLopPlotRange { this->_lower_border()->position(),
                                  this->_upper_border()->position() };
    }

    void move(double dx, double dy) override;

    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;

    inline void set_borders_tool_tip(const QString& tool_tip)
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            this->_border1->setToolTip(tool_tip);
            this->_border2->setToolTip(tool_tip);
        }
    }

    inline void set_borders_color(const QColor& color)
    {
        if (!_border1.isNull() && !_border2.isNull())
        {
            this->_border1->set_color(color);
            this->_border2->set_color(color);
        }
    }

    [[nodiscard]] inline QColor borders_color() const noexcept
    {
        if (!_border1.isNull())
        {
            return this->_border1->pen().color();
        }
        return {};
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
        if (!_border1.isNull() && !_border2.isNull())
        {
            this->_border1->replot(false);
            this->_border2->replot(immediate);
        }
        SciQLopPlotItem::replot(immediate);
    }
};

/*! \brief Vertical span that can be added to a plot
 *
 */
class SciQLopVerticalSpan : public QObject
{
    Q_OBJECT
    QPointer<VerticalSpan> _impl;

    friend class MultiPlotsVerticalSpan;

protected:
    inline void select_lower_border(bool selected) { _impl->select_lower_border(selected); }

    inline void select_upper_border(bool selected) { _impl->select_upper_border(selected); }

#ifndef BINDINGS_H
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);
#endif

public:
#ifndef BINDINGS_H
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void selectionChanged(bool);
    Q_SIGNAL void delete_requested();
#endif
    /*! \brief SciQLopVerticalSpan
     *
     * \param plot The plot where the vertical span will be added
     * \param horizontal_range The range of the span
     * \param color The color of the span
     * \param read_only If the span is read only
     * \param visible If the span is visible
     * \param tool_tip The tool tip of the span
     */
    SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
                        QColor color = QColor(100, 100, 100, 100), bool read_only = false,
                        bool visible = true, const QString& tool_tip = "");

    virtual ~SciQLopVerticalSpan() override
    {
        if (!this->_impl.isNull())
        {
            auto plot = this->parentPlot();
            if (plot->hasItem(this->_impl.data()))
            {
                plot->removeItem(this->_impl.data());
                plot->replot(QCustomPlot::rpQueuedReplot);
            }
        }
    }

    inline QCustomPlot* parentPlot() const noexcept
    {
        if (!_impl.isNull())
            return _impl->parentPlot();
        return nullptr;
    }

    /*! \brief Set the span as movable
     *
     * \param movable
     */
    inline void set_visible(bool visible)
    {
        if (!_impl.isNull())
            _impl->set_visible(visible);
    }

    /*! \brief Set the span as movable
     *
     * \param movable
     */
    inline bool visible() const noexcept
    {
        if (!_impl.isNull())
            return _impl->visible();
        return false;
    }

    inline void set_range(const SciQLopPlotRange horizontal_range)
    {
        if (!_impl.isNull())
            this->_impl->set_range(horizontal_range);
    }

    [[nodiscard]] inline SciQLopPlotRange range() const noexcept
    {
        if (!_impl.isNull())
            return this->_impl->range();
        return {};
    }

    inline void set_color(const QColor& color)
    {
        if (!_impl.isNull())
            this->_impl->set_color(color);
    }

    [[nodiscard]] inline QColor color() const
    {
        if (!_impl.isNull())
            return this->_impl->color();
        return {};
    }

    inline void set_borders_color(const QColor& color)
    {
        if (!_impl.isNull())
            this->_impl->set_borders_color(color);
    }

    [[nodiscard]] inline QColor borders_color() const noexcept
    {
        if (!_impl.isNull())
            return this->_impl->borders_color();
        return {};
    }

    inline void set_selected(bool selected)
    {
        if (!_impl.isNull())
            this->_impl->setSelected(selected);
    }

    [[nodiscard]] inline bool selected() const noexcept
    {
        if (!_impl.isNull())
            return this->_impl->selected();
        return false;
    }

    inline void set_read_only(bool read_only)
    {
        if (!_impl.isNull())
            this->_impl->setMovable(!read_only);
    }

    [[nodiscard]] inline bool read_only() const noexcept
    {
        if (!_impl.isNull())
            return !this->_impl->movable();
        return false;
    }

    inline void set_tool_tip(const QString& tool_tip)
    {
        if (!_impl.isNull())
        {
            this->_impl->setToolTip(tool_tip);
            this->_impl->set_borders_tool_tip(tool_tip);
        }
    }

    [[nodiscard]] inline QString tool_tip() const noexcept
    {
        if (!_impl.isNull())
            return this->_impl->tooltip();
        return {};
    }

    inline void replot()
    {
        if (!_impl.isNull())
            this->_impl->replot();
    }
};
