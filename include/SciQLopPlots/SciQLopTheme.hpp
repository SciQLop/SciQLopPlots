#pragma once
#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <theme.h>

class SciQLopTheme : public QObject
{
    Q_OBJECT

    QPointer<QCPTheme> m_theme;

    // Private constructor from existing QCPTheme
    explicit SciQLopTheme(QCPTheme* theme, QObject* parent = nullptr);

    friend class SciQLopPlot;

    QCPTheme* qcp_theme() const { return m_theme; }

public:
    virtual ~SciQLopTheme() override = default;

    static SciQLopTheme* light(QObject* parent = nullptr);
    static SciQLopTheme* dark(QObject* parent = nullptr);

    QColor background() const;
    QColor foreground() const;
    QColor grid() const;
    QColor sub_grid() const;
    QColor selection() const;
    QColor legend_background() const;
    QColor legend_border() const;

    void set_background(const QColor& color);
    void set_foreground(const QColor& color);
    void set_grid(const QColor& color);
    void set_sub_grid(const QColor& color);
    void set_selection(const QColor& color);
    void set_legend_background(const QColor& color);
    void set_legend_border(const QColor& color);

    QString busy_indicator_symbol() const;
    qreal busy_fade_alpha() const;
    int busy_show_delay_ms() const;
    int busy_hide_delay_ms() const;

    void set_busy_indicator_symbol(const QString& symbol);
    void set_busy_fade_alpha(qreal alpha);
    void set_busy_show_delay_ms(int ms);
    void set_busy_hide_delay_ms(int ms);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void changed();
};
