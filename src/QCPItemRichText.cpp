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
#include "SciQLopPlots/Items/QCPItemRichText.hpp"

QCPItemRichText::QCPItemRichText(QCustomPlot* parentPlot) : QCPItemText(parentPlot)
{
    m_textDocument = new QTextDocument(this);
    m_textDocument->setDefaultFont(mainFont());
    m_textDocument->setDocumentMargin(0);
    m_textDocument->setTextWidth(QWIDGETSIZE_MAX);
    m_textDocument->setUndoRedoEnabled(false);
    m_textDocument->setUseDesignMetrics(true);
    setSelectable(false);
}

QCPItemRichText::~QCPItemRichText() { }

void QCPItemRichText::setHtml(const QString& html)
{
    m_textDocument->setHtml(html);
    _boundingRect = QRect(0, 0, m_textDocument->idealWidth(), m_textDocument->size().height());
}

void QCPItemRichText::setHtml(const std::string& html)
{
    setHtml(QString::fromStdString(html));
}

void QCPItemRichText::draw(QCPPainter* painter)
{
    QPointF pos(position->pixelPosition());
    QTransform transform = painter->transform();
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);
    painter->setFont(mainFont());
    auto textRect = _boundingRect;
    QRect textBoxRect
        = textRect.adjusted(-mPadding.left(), -mPadding.top(), mPadding.right(), mPadding.bottom());
    QPointF textPos = getTextDrawPoint(QPointF(0, 0), textBoxRect,
        mPositionAlignment); // 0, 0 because the transform does the translation
    textRect.moveTopLeft(textPos.toPoint() + QPoint(mPadding.left(), mPadding.top()));
    textBoxRect.moveTopLeft(textPos.toPoint());
    int clipPad = qCeil(mainPen().widthF());
    QRect boundingRect = textBoxRect.adjusted(-clipPad, -clipPad, clipPad, clipPad);

    if (transform.mapRect(boundingRect).intersects(painter->transform().mapRect(clipRect())))
    {
        painter->setTransform(transform);
        if ((mainBrush().style() != Qt::NoBrush && mainBrush().color().alpha() != 0)
            || (mainPen().style() != Qt::NoPen && mainPen().color().alpha() != 0))
        {
            painter->setPen(mainPen());
            painter->setBrush(mainBrush());
            painter->drawRect(textBoxRect);
        }
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(mainColor()));
        painter->translate(textPos);
        painter->translate(mPadding.left(), mPadding.top());
        m_textDocument->drawContents(painter);
    }
}

QPointF QCPItemRichText::anchorPixelPosition(int anchorId) const
{
    // get actual rect points (pretty much copied from draw function):
    QPointF pos(position->pixelPosition());
    QTransform transform;
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);
    QRect textRect = _boundingRect;
    QRectF textBoxRect
        = textRect.adjusted(-mPadding.left(), -mPadding.top(), mPadding.right(), mPadding.bottom());
    QPointF textPos = getTextDrawPoint(QPointF(0, 0), textBoxRect,
        mPositionAlignment); // 0, 0 because the transform does the translation
    textBoxRect.moveTopLeft(textPos.toPoint());
    QPolygonF rectPoly = transform.map(QPolygonF(textBoxRect));

    switch (anchorId)
    {
        case aiTopLeft:
            return rectPoly.at(0);
        case aiTop:
            return (rectPoly.at(0) + rectPoly.at(1)) * 0.5;
        case aiTopRight:
            return rectPoly.at(1);
        case aiRight:
            return (rectPoly.at(1) + rectPoly.at(2)) * 0.5;
        case aiBottomRight:
            return rectPoly.at(2);
        case aiBottom:
            return (rectPoly.at(2) + rectPoly.at(3)) * 0.5;
        case aiBottomLeft:
            return rectPoly.at(3);
        case aiLeft:
            return (rectPoly.at(3) + rectPoly.at(0)) * 0.5;
    }

    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}
