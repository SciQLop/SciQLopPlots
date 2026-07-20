#include "SciQLopPlots/Products/ProductsFlatFilterModel.hpp"
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include "SciQLopPlots/Products/SubsequenceMatcher.hpp"
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include <algorithm>
#include <magic_enum/magic_enum.hpp>

ProductsFlatFilterModel::ProductsFlatFilterModel(ProductsModel* source, QObject* parent)
    : QAbstractListModel(parent), m_source(source)
{
    m_batch_timer = new QTimer(this);
    m_batch_timer->setInterval(0);
    m_batch_timer->setSingleShot(false);
    connect(m_batch_timer, &QTimer::timeout, this, &ProductsFlatFilterModel::process_batch);

    connect(m_source, &QAbstractItemModel::rowsInserted, this,
            &ProductsFlatFilterModel::rebuild);
    connect(m_source, &QAbstractItemModel::rowsRemoved, this,
            &ProductsFlatFilterModel::rebuild);
    connect(m_source, &QAbstractItemModel::modelReset, this,
            &ProductsFlatFilterModel::rebuild);
}

void ProductsFlatFilterModel::set_query(const Query& query)
{
    m_query = query;
    rebuild();
}

int ProductsFlatFilterModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_results.size();
}

QVariant ProductsFlatFilterModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return {};

    auto* node = m_results[index.row()].node;

    if (role == ProductsRelevanceScoreRole)
    {
        if (m_query.free_text_tokens.isEmpty() || m_max_score <= 0)
            return {};
        return qRound(m_results[index.row()].score * 100.0 / m_max_score);
    }

    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::UserRole:
        return node->name();
    case Qt::DecorationRole:
        return node->icon();
    case Qt::ToolTipRole:
        return node->tooltip();
    default:
        return {};
    }
}

Qt::ItemFlags ProductsFlatFilterModel::flags(const QModelIndex& index) const
{
    auto f = QAbstractListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled;
    return f;
}

QMimeData* ProductsFlatFilterModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mime = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    QStringList textParts;
    for (const auto& index : indexes)
    {
        if (!index.isValid() || index.row() >= m_results.size())
            continue;
        auto p = m_results[index.row()].node->path();
        stream << p;
        textParts.append(p.join("//"));
    }
    mime->setData(ProductsModel::mime_type(), encodedData);
    mime->setText(textParts.join("\n"));
    return mime;
}

QStringList ProductsFlatFilterModel::mimeTypes() const
{
    return { ProductsModel::mime_type() };
}

Qt::DropActions ProductsFlatFilterModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

void ProductsFlatFilterModel::rebuild()
{
    m_batch_timer->stop();
    ++m_batch_generation;

    beginResetModel();
    m_results.clear();
    m_max_score = 0;
    m_node_raw_signals.clear();
    m_signal_maxes.clear();
    endResetModel();

    m_pending_leaves.clear();
    m_pending_raw_signals.clear();
    m_pending_signal_maxes.clear();
    m_batch_cursor = 0;

    for (int i = 0; i < m_source->rowCount(); ++i)
    {
        auto idx = m_source->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            collect_all_leaves(node, m_pending_leaves);
    }

    if (!m_pending_leaves.isEmpty())
        m_batch_timer->start();
}

QHash<QString, QString> ProductsFlatFilterModel::corpus_snapshot() const
{
    QList<LeafEntry> leaves;
    for (int i = 0; i < m_source->rowCount(); ++i)
    {
        auto idx = m_source->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            collect_all_leaves(node, leaves);
    }
    QHash<QString, QString> snapshot;
    snapshot.reserve(leaves.size());
    for (const auto& leaf : leaves)
        snapshot.insert(leaf.path_text, leaf.meta_text);
    return snapshot;
}

void ProductsFlatFilterModel::collect_all_leaves(ProductsModelNode* node,
                                                  QList<LeafEntry>& out) const
{
    if (node->node_type() == ProductsModelNodeType::PARAMETER)
    {
        out.append({ node, node->path().join(' '), node->raw_text() });
        return;
    }
    for (auto* child : node->children_nodes())
        collect_all_leaves(child, out);
}

void ProductsFlatFilterModel::process_batch()
{
    int generation = m_batch_generation;
    int end = std::min(m_batch_cursor + BATCH_SIZE, static_cast<int>(m_pending_leaves.size()));

    QList<ScoredNode> batch_results;
    for (int i = m_batch_cursor; i < end; ++i)
    {
        if (m_batch_generation != generation)
            return;

        auto& [node, path_text, meta_text] = m_pending_leaves[i];
        if (!filters_match(node))
            continue;

        QHash<QString, double> raw_signals;
        raw_signals.insert(QStringLiteral("fuzzy"),
                            static_cast<double>(free_text_score(path_text, meta_text)));
        for (const auto& signal_name : m_score_signals.registered_signals())
        {
            auto value = m_score_signals.score_for(signal_name, path_text);
            if (value)
                raw_signals.insert(signal_name, *value);
        }

        m_pending_raw_signals.insert(node, raw_signals);
        for (auto it = raw_signals.constBegin(); it != raw_signals.constEnd(); ++it)
            m_pending_signal_maxes[it.key()]
                = std::max(m_pending_signal_maxes.value(it.key(), 0.0), it.value());

        // Provisional merge using the running max-so-far, so results keep
        // streaming in progressively during the scan (as today) -- raw
        // scores >0 always normalize to >0 regardless of which max is
        // used, so a leaf shown here never has to be retracted, only
        // re-ranked, once finalize_batch() recomputes with the true maxes.
        auto merged = merge_scores(raw_signals, m_merge_strategy, m_signal_weights,
                                    m_pending_signal_maxes, m_override_signal);
        if (merged && *merged > 0.0)
        {
            batch_results.append({ node, *merged });
            m_max_score = std::max(m_max_score, *merged);
        }
    }

    if (!batch_results.isEmpty())
    {
        int first = m_results.size();
        beginInsertRows(QModelIndex(), first, first + batch_results.size() - 1);
        m_results.append(batch_results);
        endInsertRows();
    }

    m_batch_cursor = end;

    if (m_batch_cursor >= m_pending_leaves.size())
        finalize_batch();
}

void ProductsFlatFilterModel::finalize_batch()
{
    m_batch_timer->stop();

    m_node_raw_signals = m_pending_raw_signals;
    m_signal_maxes = m_pending_signal_maxes;
    m_pending_raw_signals.clear();
    m_pending_signal_maxes.clear();

    m_max_score = 0;
    for (auto& scored : m_results)
    {
        auto merged = merge_scores(m_node_raw_signals.value(scored.node), m_merge_strategy,
                                    m_signal_weights, m_signal_maxes, m_override_signal);
        scored.score = merged.value_or(0.0);
        m_max_score = std::max(m_max_score, scored.score);
    }

    sort_and_remap_results();
}

// Re-runs merge_scores() over every leaf that has ever passed
// filters_match() for the current query (m_node_raw_signals is a superset
// of m_results: it includes leaves whose merged score is currently 0), using
// the current strategy/weights/override -- no re-scoring of free text
// needed. Unlike finalize_batch(), the row *set* can genuinely change here
// (e.g. switching to Override can shrink or grow which leaves match), so
// this uses a full model reset rather than the persistent-index-preserving
// sort finalize_batch() uses for its narrower "same rows, new order" case.
void ProductsFlatFilterModel::remerge_committed()
{
    if (m_node_raw_signals.isEmpty())
        return;

    beginResetModel();
    m_results.clear();
    m_max_score = 0;
    for (auto it = m_node_raw_signals.constBegin(); it != m_node_raw_signals.constEnd(); ++it)
    {
        auto merged = merge_scores(it.value(), m_merge_strategy, m_signal_weights, m_signal_maxes,
                                    m_override_signal);
        if (merged && *merged > 0.0)
        {
            m_results.append({ it.key(), *merged });
            m_max_score = std::max(m_max_score, *merged);
        }
    }
    std::sort(m_results.begin(), m_results.end(),
              [](const ScoredNode& a, const ScoredNode& b) { return a.score > b.score; });
    endResetModel();
}

void ProductsFlatFilterModel::sort_and_remap_results()
{
    // Sort by score descending — emit layoutChanged so views update order.
    // The layout-change contract requires remapping persistent indexes
    // (selections taken while batches streamed) or they silently retarget
    // to whatever lands on their old row after the sort.
    emit layoutAboutToBeChanged();
    const QModelIndexList old_indexes = persistentIndexList();
    QList<ProductsModelNode*> old_nodes;
    old_nodes.reserve(old_indexes.size());
    for (const auto& idx : old_indexes)
        old_nodes.append(m_results[idx.row()].node);

    std::sort(m_results.begin(), m_results.end(),
              [](const ScoredNode& a, const ScoredNode& b) { return a.score > b.score; });

    if (!old_indexes.isEmpty())
    {
        QHash<ProductsModelNode*, int> new_rows;
        new_rows.reserve(m_results.size());
        for (int row = 0; row < m_results.size(); ++row)
            new_rows.insert(m_results[row].node, row);
        QModelIndexList new_indexes;
        new_indexes.reserve(old_indexes.size());
        for (qsizetype i = 0; i < old_indexes.size(); ++i)
            new_indexes.append(
                index(new_rows.value(old_nodes[i], -1), old_indexes[i].column()));
        changePersistentIndexList(old_indexes, new_indexes);
    }
    emit layoutChanged();
}

bool ProductsFlatFilterModel::filters_match(ProductsModelNode* node) const
{
    for (const auto& filter : m_query.filters)
    {
        if (filter.field.compare("after", Qt::CaseInsensitive) == 0)
        {
            if (!filter.parsed_date.isValid())
                return false;
            auto stop_date = node->metadata("stop_date").toString();
            if (stop_date.isEmpty())
                return false;
            auto node_date = QDateTime::fromString(stop_date, Qt::ISODate);
            if (!node_date.isValid() || node_date < filter.parsed_date)
                return false;
            continue;
        }

        if (filter.field.compare("before", Qt::CaseInsensitive) == 0)
        {
            if (!filter.parsed_date.isValid())
                return false;
            auto start_date = node->metadata("start_date").toString();
            if (start_date.isEmpty())
                return false;
            auto node_date = QDateTime::fromString(start_date, Qt::ISODate);
            if (!node_date.isValid() || node_date > filter.parsed_date)
                return false;
            continue;
        }

        QString node_value;
        if (filter.field.compare("provider", Qt::CaseInsensitive) == 0)
            node_value = node->provider();
        else if (filter.field.compare("type", Qt::CaseInsensitive) == 0)
            node_value = QString::fromStdString(
                             std::string(magic_enum::enum_name(node->parameter_type())))
                             .toLower();
        else
            node_value = node->metadata(filter.field).toString();

        if (node_value.compare(filter.value, Qt::CaseInsensitive) != 0)
            return false;
    }
    return true;
}

int ProductsFlatFilterModel::free_text_score(const QString& path_text,
                                              const QString& meta_text) const
{
    if (m_query.free_text_tokens.isEmpty())
        return 1;

    int total = 0;
    for (const auto& token : m_query.free_text_tokens)
    {
        int s = std::max(subsequence_score(token, path_text), subsequence_score(token, meta_text));
        if (s == 0)
            return 0;
        total += s;
    }
    return total;
}
