#include "SciQLopPlots/Products/ProductsScoreDelegate.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include <QFontMetrics>
#include <QPainter>

ProductsScoreDelegate::ProductsScoreDelegate(QObject* parent) : QStyledItemDelegate(parent) { }

void ProductsScoreDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    QVariant relevance = index.data(ProductsRelevanceScoreRole);
    QVariant coverage = index.data(ProductsCoverageRole);
    if (!relevance.isValid() && !coverage.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const int right_margin = 8;
    QString text;
    bool draw_dot = false;
    QColor dot_color;

    if (relevance.isValid())
    {
        int percent = relevance.toInt();
        // Continuous green (100%) -> red (0%) gradient in HSV hue space --
        // not fixed traffic-light bands, since this % is relative-to-best,
        // not an absolute pass/fail cutoff.
        dot_color = QColor::fromHsv(int(120 * percent / 100.0), 200, 200);
        text = QString("%1%").arg(percent);
        draw_dot = true;
    }
    else
    {
        text = coverage.toString();
    }

    QFontMetrics fm(option.font);
    int text_width = fm.horizontalAdvance(text);
    QRect text_rect(option.rect.right() - right_margin - text_width, option.rect.top(),
                     text_width, option.rect.height());

    if (draw_dot)
    {
        int dot_diameter = option.rect.height() / 3;
        QRect dot_rect(text_rect.left() - dot_diameter - 4,
                        option.rect.top() + (option.rect.height() - dot_diameter) / 2,
                        dot_diameter, dot_diameter);
        painter->setBrush(dot_color);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(dot_rect);
    }

    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignRight, text);

    painter->restore();
}
