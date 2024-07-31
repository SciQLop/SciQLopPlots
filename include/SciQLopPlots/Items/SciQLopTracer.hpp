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
#pragma once
#include <SciQLopPlots/Items/QCPItemRichText.hpp>
#include <qcustomplot.h>

enum class PlotableType
{
    Graph,
    Curve,
    ColorMap,
    None
};

enum class PlotQuadrant
{
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    None
};

class QCPAbstractPlottableDataLocator
{
    QCPAbstractPlottable* m_plottable = nullptr;
    PlotableType m_plotableType = PlotableType::None;
    double m_key = std::nan("");
    double m_value = std::nan("");
    double m_data = std::nan("");
    PlotQuadrant m_quadrant = PlotQuadrant::None;

public:
    virtual ~QCPAbstractPlottableDataLocator() = default;

    inline QCPAbstractPlottable* plottable() const noexcept { return m_plottable; }
    void set_plottable(QCPAbstractPlottable* plottable) noexcept;


    void setPosition(const QPointF& pos);


    inline double key() const noexcept { return m_key; }
    inline double value() const noexcept { return m_value; }
    inline double data() const noexcept
    {
        return m_data;
    } // color map value at (key; value) position

    inline PlotableType plotable_type() const noexcept { return m_plotableType; }
    inline PlotQuadrant quadrant() const noexcept { return m_quadrant; }

    static std::tuple<double, double, double> locate_data_graph(
        const QPointF& pos, QCPGraph* graph);
    static std::tuple<double, double, double> locate_data_curve(
        const QPointF& pos, QCPCurve* curve);
    static std::tuple<double, double, double> locate_data_colormap(
        const QPointF& pos, QCPColorMap* colormap);
};

class SciQLopTracer : public QCPAbstractItem
{
    Q_OBJECT

    QCPAbstractPlottableDataLocator m_locator;
    QPen m_Pen, m_SelectedPen;
    QBrush m_Brush, m_SelectedBrush;
    double m_Size;
    QCPItemTracer::TracerStyle m_Style;

    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
    Q_PROPERTY(double size READ size WRITE setSize)
    Q_PROPERTY(QCPItemTracer::TracerStyle style READ style WRITE setStyle)
    Q_PROPERTY(QCPAbstractPlottable* plotable READ plotable WRITE setPlotable)
    Q_PROPERTY(double key READ key)
    Q_PROPERTY(double value READ value)
    Q_PROPERTY(double data READ data)

    QPen mainPen() const;
    QBrush mainBrush() const;

public:
    using TracerStyle = QCPItemTracer::TracerStyle;

    SciQLopTracer(QCustomPlot* parentPlot);
    virtual ~SciQLopTracer() override { }


    QPen pen() const { return m_Pen; }
    QPen selectedPen() const { return m_SelectedPen; }
    QBrush brush() const { return m_Brush; }
    QBrush selectedBrush() const { return m_SelectedBrush; }
    double size() const { return m_Size; }
    TracerStyle style() const { return m_Style; }
    inline double key() const { return m_locator.key(); }
    inline double value() const { return m_locator.value(); }
    inline double data() const // color map value at (key; value) position
    {
        return m_locator.data();
    }
    inline PlotQuadrant quadrant() const { return m_locator.quadrant(); }
    QCPAbstractPlottable* plotable() const { return m_locator.plottable(); }

    inline void setPen(const QPen& pen) { m_Pen = pen; }
    inline void setSelectedPen(const QPen& pen) { m_SelectedPen = pen; }
    inline void setBrush(const QBrush& brush) { m_Brush = brush; }
    inline void setSelectedBrush(const QBrush& brush) { m_SelectedBrush = brush; }
    inline void setSize(double size) { m_Size = size; }
    inline void setStyle(TracerStyle style) { m_Style = style; }
    void setPlotable(QCPAbstractPlottable* plottable);

    void updatePosition(const QPointF& pos);

    virtual double selectTest(
        const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const Q_DECL_OVERRIDE;

    QCPAxis* keyAxis() const;
    QCPAxis* valueAxis() const;

    QCPItemPosition* const position;

protected:
    virtual void draw(QCPPainter* painter) override;
};

class TracerWithToolTip : QObject
{
    Q_OBJECT
    QCPItemRichText* m_tooltip = nullptr;
    SciQLopTracer* m_tracer = nullptr;
    double m_x = std::nan("");
    double m_y = std::nan("");
    double m_data = std::nan("");

public:
    TracerWithToolTip(QCustomPlot* parent = nullptr);

    virtual ~TracerWithToolTip();

    void update_position(const QPointF& pos, bool replot = true);

    inline void set_visible(bool visible)
    {
        m_tracer->setVisible(visible);
        m_tooltip->setVisible(visible);
    }

    inline bool visible() const noexcept { return m_tracer->visible(); }
    inline void replot() { m_tracer->layer()->replot(); }

    inline void set_plotable(QCPAbstractPlottable* plotable)
    {
        m_tracer->setPlotable(plotable);
        if (plotable == nullptr)
        {
            set_visible(false);
            replot();
        }
    }
};
