/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
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
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/enums.hpp"
#include <cmath>
#include <optional>

class StraightLine : public impl::SciQLopPlotItem<QCPItemStraightLine>, public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

    Qt::Orientations m_orientation;
    Coordinates m_coordinates;
    std::optional<double> m_min_value;
    std::optional<double> m_max_value;

    inline double _clamp(double pos) const
    {
        if (std::isnan(pos))
        {
            if (m_min_value)
                return *m_min_value;
            if (m_max_value)
                return *m_max_value;
            return 0.0;
        }
        // If both bounds are set but inverted (min > max), the user-most-recent
        // setter wins by clamping to the tighter bound: max takes precedence
        // because hitting the upper limit is the more common UI intent.
        if (m_min_value && m_max_value && *m_min_value > *m_max_value)
        {
            // Inverted bounds — fall back to the upper bound to keep callers
            // safe instead of silently violating max as the unconditional
            // ladder would.
            return *m_max_value;
        }
        if (m_min_value && pos < *m_min_value)
            return *m_min_value;
        if (m_max_value && pos > *m_max_value)
            return *m_max_value;
        return pos;
    }

public:

    inline StraightLine(QCustomPlot* plot, const double position, const bool movable = false,
                        Coordinates coordinates = Coordinates::Data,
                        Qt::Orientation orientation = Qt::Orientation::Vertical)
            : impl::SciQLopPlotItem<QCPItemStraightLine> { plot }
            , m_orientation { orientation }
            , m_coordinates { coordinates }
    {
        if (orientation == Qt::Orientation::Vertical)
        {
            this->point1->setCoords(position, 0);
            this->point2->setCoords(position, 1);
        }
        else
        {
            this->point1->setCoords(0, position);
            this->point2->setCoords(1, position);
        }
        this->setMovable(movable);
        if (coordinates == Coordinates::Data)
        {
            this->point1->setType(QCPItemPosition::ptPlotCoords);
            this->point2->setType(QCPItemPosition::ptPlotCoords);
        }
        else
        {
            this->point1->setType(QCPItemPosition::ptAbsolute);
            this->point2->setType(QCPItemPosition::ptAbsolute);
        }
    }

    virtual ~StraightLine() { }

    void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;

    virtual void move(double dx, double dy) override;
    void set_position(double pos);
    [[nodiscard]] double position() const;

    inline void set_min_value(double min) { m_min_value = min; }
    inline void clear_min_value() { m_min_value.reset(); }
    [[nodiscard]] inline std::optional<double> min_value() const { return m_min_value; }

    inline void set_max_value(double max) { m_max_value = max; }
    inline void clear_max_value() { m_max_value.reset(); }
    [[nodiscard]] inline std::optional<double> max_value() const { return m_max_value; }

    void set_color(const QColor& color);
    [[nodiscard]] QColor color() const;

    void set_line_width(double width);
    [[nodiscard]] double line_width() const;

    void set_line_style(Qt::PenStyle style);
    [[nodiscard]] Qt::PenStyle line_style() const;

    virtual QCursor cursor(QMouseEvent* event) const noexcept override
    {
        if (!_movable)
            return Qt::ArrowCursor;
        if (m_orientation == Qt::Orientation::Vertical)
            return Qt::SizeHorCursor;
        return Qt::SizeVerCursor;
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void moved(double new_position);
};

class SciQLopStraightLine : public QObject
{
    Q_OBJECT

    QPointer<StraightLine> m_line;

public:
    virtual ~SciQLopStraightLine()
    {
        if (!m_line.isNull())
            if (auto* plot = m_line->parentPlot())
                plot->removeItem(m_line);
    }

    /*!
     * \brief SciQLopStraightLine
     * \param plot The plot to which the item will be added
     * \param position The position of the line
     * \param movable If the line can be moved by the user
     * \param coordinates The coordinates system in which the position is defined (Pixels or Data)
     * \param orientation The orientation of the line (Vertical or Horizontal)
     */
    SciQLopStraightLine(SciQLopPlot* plot, const double position, const bool movable = false,
                        Coordinates coordinates = Coordinates::Data,
                        Qt::Orientation orientation = Qt::Orientation::Vertical)
    {
        m_line = new StraightLine(plot->qcp_plot(), position, movable, coordinates, orientation);
        connect(m_line, &StraightLine::moved, this, &SciQLopStraightLine::position_changed);
    }

    void set_position(double pos);

    [[nodiscard]] inline double position() const
    {
        if (!m_line.isNull())
            return m_line->position();
        return std::numeric_limits<double>::quiet_NaN();
    }

    inline void set_color(const QColor& color)
    {
        if (!m_line.isNull())
            this->m_line->set_color(color);
    }

    [[nodiscard]] inline QColor color() const
    {
        if (!m_line.isNull())
            return this->m_line->color();
        return {};
    }

    void inline set_line_width(double width)
    {
        if (!m_line.isNull())
            this->m_line->set_line_width(width);
    }

    [[nodiscard]] inline double line_width() const
    {
        if (!m_line.isNull())
            return this->m_line->line_width();
        return std::numeric_limits<double>::quiet_NaN();
    }

    inline void set_line_style(Qt::PenStyle style)
    {
        if (!m_line.isNull())
            this->m_line->set_line_style(style);
    }

    [[nodiscard]] inline Qt::PenStyle line_style() const
    {
        if (!m_line.isNull())
            return this->m_line->line_style();
        return Qt::SolidLine;
    }

    inline void set_min_value(double min)
    {
        if (!m_line.isNull())
            m_line->set_min_value(min);
    }

    inline void clear_min_value()
    {
        if (!m_line.isNull())
            m_line->clear_min_value();
    }

    inline void set_max_value(double max)
    {
        if (!m_line.isNull())
            m_line->set_max_value(max);
    }

    inline void clear_max_value()
    {
        if (!m_line.isNull())
            m_line->clear_max_value();
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void position_changed(double new_position);
};

class SciQLopVerticalLine : public SciQLopStraightLine
{
    Q_OBJECT

public:
    /*!
     * \brief SciQLopVerticalLine
     * \param plot The plot to which the item will be added
     * \param position The position of the line
     * \param movable If the line can be moved by the user
     * \param coordinates The coordinates system in which the position is defined (Pixels or Data)
     */
    SciQLopVerticalLine(SciQLopPlot* plot, const double position, const bool movable = false,
                        Coordinates coordinates = Coordinates::Data)
            : SciQLopStraightLine(plot, position, movable, coordinates, Qt::Orientation::Vertical)
    {
    }
};

class SciQLopHorizontalLine : public SciQLopStraightLine
{
    Q_OBJECT

public:
    /*!
     * \brief SciQLopHorizontalLine
     * \param plot The plot to which the item will be added
     * \param position The position of the line
     * \param movable If the line can be moved by the user
     * \param coordinates The coordinates system in which the position is defined (Pixels or Data)
     */
    SciQLopHorizontalLine(SciQLopPlot* plot, const double position, const bool movable = false,
                          Coordinates coordinates = Coordinates::Data)
            : SciQLopStraightLine(plot, position, movable, coordinates, Qt::Orientation::Horizontal)
    {
    }
};
