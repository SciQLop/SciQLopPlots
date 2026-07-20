#include "SciQLopPlots/Products/ScoreSignalRegistry.hpp"
#include <QMutexLocker>

void ScoreSignalRegistry::set_scores(const QString& signal_name,
                                      const QHash<QString, QVariant>& scores)
{
    auto converted = std::make_shared<QHash<QString, double>>();
    converted->reserve(scores.size());
    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
        converted->insert(it.key(), it.value().toDouble());

    QMutexLocker locker(&m_mutex);
    m_scores.insert(signal_name, converted);
}

void ScoreSignalRegistry::set_signal_enabled(const QString& signal_name, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enabled.insert(signal_name, enabled);
}

bool ScoreSignalRegistry::signal_enabled(const QString& signal_name) const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled.value(signal_name, false);
}

QStringList ScoreSignalRegistry::registered_signals() const
{
    QMutexLocker locker(&m_mutex);
    return m_scores.keys();
}

std::optional<double> ScoreSignalRegistry::score_for(const QString& signal_name,
                                                      const QString& path_key) const
{
    std::shared_ptr<const QHash<QString, double>> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled.value(signal_name, false))
            return std::nullopt;
        snapshot = m_scores.value(signal_name);
    }
    if (!snapshot)
        return std::nullopt;
    auto it = snapshot->constFind(path_key);
    if (it == snapshot->constEnd())
        return std::nullopt;
    return it.value();
}
