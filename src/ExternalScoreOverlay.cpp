#include "SciQLopPlots/Products/ExternalScoreOverlay.hpp"
#include <QMutexLocker>

void ExternalScoreOverlay::set_scores(const QHash<QString, QVariant>& scores)
{
    auto converted = std::make_shared<QHash<QString, double>>();
    converted->reserve(scores.size());
    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
        converted->insert(it.key(), it.value().toDouble());

    QMutexLocker locker(&m_mutex);
    m_scores = converted;
}

void ExternalScoreOverlay::set_enabled(bool enabled) noexcept
{
    m_enabled.store(enabled, std::memory_order_relaxed);
}

bool ExternalScoreOverlay::enabled() const noexcept
{
    return m_enabled.load(std::memory_order_relaxed);
}

double ExternalScoreOverlay::score_for(const QString& path_key) const
{
    if (!enabled())
        return 0.0;
    std::shared_ptr<const QHash<QString, double>> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        snapshot = m_scores;
    }
    if (!snapshot)
        return 0.0;
    return snapshot->value(path_key, 0.0);
}
