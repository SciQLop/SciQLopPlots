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

class StraightLine : public impl::SciQLopPlotItem<QCPItemStraightLine>, public impl::SciQlopItemWithToolTip
{
    Q_OBJECT

    Qt::Orientations m_orientation;
    Coordinates m_coordinates;

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

    virtual void move(double dx, double dy) override;
    void set_position(double pos);
    [[nodiscard]] double position() const;

    void set_color(const QColor& color);
    [[nodiscard]] QColor color() const;

    void set_line_width(double width);
    [[nodiscard]] double line_width() const;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void moved(double new_position);
};

/*!
 * \brief The SciQLopPixmapItem class
 */
class SciQLopStraightLine : public QObject
{
    Q_OBJECT

    QPointer<StraightLine> m_line;

public:
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
