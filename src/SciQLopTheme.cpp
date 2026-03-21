#include <SciQLopPlots/SciQLopTheme.hpp>

SciQLopTheme::SciQLopTheme(QCPTheme* theme, QObject* parent)
    : QObject(parent), m_theme(theme)
{
    if (m_theme)
    {
        m_theme->setParent(this);
        connect(m_theme, &QCPTheme::changed, this, &SciQLopTheme::changed);
    }
}

SciQLopTheme* SciQLopTheme::light(QObject* parent)
{
    return new SciQLopTheme(QCPTheme::light(), parent);
}

SciQLopTheme* SciQLopTheme::dark(QObject* parent)
{
    return new SciQLopTheme(QCPTheme::dark(), parent);
}

// -- Color getters --
QColor SciQLopTheme::background() const { return m_theme ? m_theme->background() : QColor(); }
QColor SciQLopTheme::foreground() const { return m_theme ? m_theme->foreground() : QColor(); }
QColor SciQLopTheme::grid() const { return m_theme ? m_theme->grid() : QColor(); }
QColor SciQLopTheme::sub_grid() const { return m_theme ? m_theme->subGrid() : QColor(); }
QColor SciQLopTheme::selection() const { return m_theme ? m_theme->selection() : QColor(); }
QColor SciQLopTheme::legend_background() const { return m_theme ? m_theme->legendBackground() : QColor(); }
QColor SciQLopTheme::legend_border() const { return m_theme ? m_theme->legendBorder() : QColor(); }

// -- Color setters --
void SciQLopTheme::set_background(const QColor& c) { if (m_theme) m_theme->setBackground(c); }
void SciQLopTheme::set_foreground(const QColor& c) { if (m_theme) m_theme->setForeground(c); }
void SciQLopTheme::set_grid(const QColor& c) { if (m_theme) m_theme->setGrid(c); }
void SciQLopTheme::set_sub_grid(const QColor& c) { if (m_theme) m_theme->setSubGrid(c); }
void SciQLopTheme::set_selection(const QColor& c) { if (m_theme) m_theme->setSelection(c); }
void SciQLopTheme::set_legend_background(const QColor& c) { if (m_theme) m_theme->setLegendBackground(c); }
void SciQLopTheme::set_legend_border(const QColor& c) { if (m_theme) m_theme->setLegendBorder(c); }

// -- Busy indicator getters --
QString SciQLopTheme::busy_indicator_symbol() const { return m_theme ? m_theme->busyIndicatorSymbol() : QString(); }
qreal SciQLopTheme::busy_fade_alpha() const { return m_theme ? m_theme->busyFadeAlpha() : 0.3; }
int SciQLopTheme::busy_show_delay_ms() const { return m_theme ? m_theme->busyShowDelayMs() : 500; }
int SciQLopTheme::busy_hide_delay_ms() const { return m_theme ? m_theme->busyHideDelayMs() : 500; }

// -- Busy indicator setters --
void SciQLopTheme::set_busy_indicator_symbol(const QString& s) { if (m_theme) m_theme->setBusyIndicatorSymbol(s); }
void SciQLopTheme::set_busy_fade_alpha(qreal a) { if (m_theme) m_theme->setBusyFadeAlpha(a); }
void SciQLopTheme::set_busy_show_delay_ms(int ms) { if (m_theme) m_theme->setBusyShowDelayMs(ms); }
void SciQLopTheme::set_busy_hide_delay_ms(int ms) { if (m_theme) m_theme->setBusyHideDelayMs(ms); }
