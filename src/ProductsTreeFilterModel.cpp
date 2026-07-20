#include "SciQLopPlots/Products/ProductsTreeFilterModel.hpp"
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include "SciQLopPlots/Products/SubsequenceMatcher.hpp"
#include <QDateTime>
#include <QTimer>
#include <algorithm>
#include <magic_enum/magic_enum.hpp>

ProductsTreeFilterModel::ProductsTreeFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_batch_timer = new QTimer(this);
    m_batch_timer->setInterval(0);
    m_batch_timer->setSingleShot(false);
    connect(m_batch_timer, &QTimer::timeout, this, &ProductsTreeFilterModel::process_score_batch);
}

void ProductsTreeFilterModel::set_query(const Query& query)
{
    m_pending_query = query;
    start_scoring();
}

void ProductsTreeFilterModel::set_max_score_tiers(int max_tiers)
{
    max_tiers = std::max(1, max_tiers);
    if (max_tiers == m_max_score_tiers)
        return;

    m_max_score_tiers = max_tiers;

    // Only the tier *selection* changes -- every leaf's score is already
    // cached in m_node_scores from the last committed query, so this is a
    // cheap re-rank over the (small) set of distinct scores, not a new
    // full-corpus text-scoring pass. Safe to stay synchronous.
    beginFilterChange();
    apply_cutoff_and_coverage(m_node_scores, m_distinct_scores, m_max_score);
    endFilterChange();
}

void ProductsTreeFilterModel::setSourceModel(QAbstractItemModel* source_model)
{
    if (auto* old = sourceModel())
        disconnect(old, nullptr, this, nullptr);

    QSortFilterProxyModel::setSourceModel(source_model);

    if (source_model)
    {
        connect(source_model, &QAbstractItemModel::rowsInserted, this,
                &ProductsTreeFilterModel::on_source_structure_changed);
        connect(source_model, &QAbstractItemModel::rowsRemoved, this,
                &ProductsTreeFilterModel::on_source_structure_changed);
        connect(source_model, &QAbstractItemModel::modelReset, this,
                &ProductsTreeFilterModel::on_source_structure_changed);
    }
    on_source_structure_changed();
}

void ProductsTreeFilterModel::on_source_structure_changed()
{
    recompute_total_leaf_counts();
    m_pending_query = m_query;
    start_scoring();
}

void ProductsTreeFilterModel::recompute_total_leaf_counts()
{
    m_coverage.clear();

    auto* source = sourceModel();
    if (!source)
        return;

    QList<ProductsModelNode*> leaves;
    for (int i = 0; i < source->rowCount(); ++i)
    {
        auto idx = source->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            collect_all_leaves(node, leaves);
    }

    for (auto* node : leaves)
        for (auto* ancestor = node->parent_node(); ancestor != nullptr;
             ancestor = ancestor->parent_node())
            m_coverage[ancestor].total += 1;
}

// Scoring is chunked across 0ms QTimer ticks (see start_scoring/
// process_score_batch below) instead of walking the whole corpus
// synchronously here, then swapped into the committed state in one shot by
// finish_scoring() -- see the m_query/m_pending_query split in the header
// for why filterAcceptsRow() and data() never need to know a scoring pass
// is in flight.
bool ProductsTreeFilterModel::filterAcceptsRow(int source_row,
                                                const QModelIndex& source_parent) const
{
    if (m_query.free_text_tokens.isEmpty() && m_query.filters.isEmpty())
        return true;

    auto index = sourceModel()->index(source_row, 0, source_parent);
    auto* node = static_cast<ProductsModelNode*>(index.internalPointer());
    if (!node)
        return false;

    // Only PARAMETER leaves can independently satisfy a query — folders and
    // any other non-leaf node stay visible exclusively via
    // setRecursiveFilteringEnabled(true) when a descendant leaf matches,
    // never because their own (often short, cheap-to-match) path text
    // happens to score well. Mirrors ProductsFlatFilterModel, which never
    // considers non-PARAMETER nodes as candidates in the first place.
    if (node->node_type() != ProductsModelNodeType::PARAMETER)
        return false;

    auto it = m_node_scores.constFind(node);
    return it != m_node_scores.constEnd() && it.value() >= m_score_cutoff;
}

QVariant ProductsTreeFilterModel::data(const QModelIndex& index, int role) const
{
    if (role == ProductsRelevanceScoreRole)
    {
        auto* node = static_cast<ProductsModelNode*>(mapToSource(index).internalPointer());
        if (!node || node->node_type() != ProductsModelNodeType::PARAMETER)
            return {};
        if (m_query.free_text_tokens.isEmpty() || m_max_score <= 0)
            return {};
        auto it = m_node_scores.constFind(node);
        if (it == m_node_scores.constEnd() || it.value() <= 0)
            return {};
        return qRound(it.value() * 100.0 / m_max_score);
    }
    if (role == ProductsCoverageRole)
    {
        if (m_query.free_text_tokens.isEmpty())
            return {};
        auto* node = static_cast<ProductsModelNode*>(mapToSource(index).internalPointer());
        if (!node || node->node_type() == ProductsModelNodeType::PARAMETER)
            return {};
        auto it = m_coverage.constFind(node);
        if (it == m_coverage.constEnd() || it.value().total == 0)
            return {};
        int matched = it.value().matched;
        int total = it.value().total;
        return QString("%1/%2 (%3%)")
            .arg(matched).arg(total).arg(qRound(matched * 100.0 / total));
    }
    return QSortFilterProxyModel::data(index, role);
}

void ProductsTreeFilterModel::collect_all_leaves(ProductsModelNode* node,
                                                  QList<ProductsModelNode*>& out) const
{
    if (node->node_type() == ProductsModelNodeType::PARAMETER)
    {
        out.append(node);
        return;
    }
    for (auto* child : node->children_nodes())
        collect_all_leaves(child, out);
}

void ProductsTreeFilterModel::start_scoring()
{
    m_batch_timer->stop();
    ++m_batch_generation;

    m_pending_raw_signals.clear();
    m_pending_signal_maxes.clear();
    m_pending_leaves.clear();

    auto* source = sourceModel();
    if (source)
    {
        for (int i = 0; i < source->rowCount(); ++i)
        {
            auto idx = source->index(i, 0);
            auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
            if (node)
                collect_all_leaves(node, m_pending_leaves);
        }
    }

    m_batch_cursor = 0;
    if (m_pending_leaves.isEmpty())
        finish_scoring();
    else
        m_batch_timer->start();
}

// Runs on the UI thread, but bounded to BATCH_SIZE leaves per tick so the
// event loop always gets to breathe (repaint, handle input) between
// batches — this is the actual fix for the freeze: the old code did this
// same DP scoring work but for the *entire* corpus, *synchronously*, up to
// three times per query change (see git history on this file).
void ProductsTreeFilterModel::process_score_batch()
{
    int generation = m_batch_generation;
    int end = std::min(m_batch_cursor + BATCH_SIZE, static_cast<int>(m_pending_leaves.size()));

    for (int i = m_batch_cursor; i < end; ++i)
    {
        if (m_batch_generation != generation)
            return;

        auto* node = m_pending_leaves[i];
        if (!filters_match(node, m_pending_query))
            continue;

        QHash<QString, double> raw_signals;
        raw_signals.insert(QStringLiteral("fuzzy"),
                            static_cast<double>(free_text_score(node, m_pending_query)));
        for (const auto& signal_name : m_score_signals.registered_signals())
        {
            auto value = m_score_signals.score_for(signal_name, node->path().join(' '));
            if (value)
                raw_signals.insert(signal_name, *value);
        }

        m_pending_raw_signals.insert(node, raw_signals);
        for (auto it = raw_signals.constBegin(); it != raw_signals.constEnd(); ++it)
            m_pending_signal_maxes[it.key()]
                = std::max(m_pending_signal_maxes.value(it.key(), 0.0), it.value());
    }

    m_batch_cursor = end;

    if (m_batch_cursor >= m_pending_leaves.size())
    {
        m_batch_timer->stop();
        finish_scoring();
    }
}

void ProductsTreeFilterModel::finish_scoring()
{
    beginFilterChange();
    m_query = m_pending_query;
    m_node_raw_signals = m_pending_raw_signals;
    m_signal_maxes = m_pending_signal_maxes;

    QHash<ProductsModelNode*, double> merged_scores;
    QSet<double> distinct_scores;
    double max_score = 0;
    for (auto it = m_node_raw_signals.constBegin(); it != m_node_raw_signals.constEnd(); ++it)
    {
        auto merged = merge_scores(it.value(), m_merge_strategy, m_signal_weights, m_signal_maxes,
                                    m_override_signal);
        if (merged && *merged > 0.0)
        {
            merged_scores.insert(it.key(), *merged);
            distinct_scores.insert(*merged);
            max_score = std::max(max_score, *merged);
        }
    }

    apply_cutoff_and_coverage(merged_scores, distinct_scores, max_score);
    endFilterChange();
}

// Re-runs merge_scores() over every already-scored leaf's cached raw
// signals, then re-applies the tier cutoff -- no re-scoring of free text
// needed, so this stays synchronous even for a large corpus (same
// rationale as set_max_score_tiers()).
void ProductsTreeFilterModel::remerge_committed()
{
    if (m_node_raw_signals.isEmpty())
        return;

    QHash<ProductsModelNode*, double> merged_scores;
    QSet<double> distinct_scores;
    double max_score = 0;
    for (auto it = m_node_raw_signals.constBegin(); it != m_node_raw_signals.constEnd(); ++it)
    {
        auto merged = merge_scores(it.value(), m_merge_strategy, m_signal_weights, m_signal_maxes,
                                    m_override_signal);
        if (merged && *merged > 0.0)
        {
            merged_scores.insert(it.key(), *merged);
            distinct_scores.insert(*merged);
            max_score = std::max(max_score, *merged);
        }
    }

    beginFilterChange();
    apply_cutoff_and_coverage(merged_scores, distinct_scores, max_score);
    endFilterChange();
}

// Raw free-text scores are a small, coarse, query/corpus-dependent scale
// (see free_text_score's length-penalty normalization) — there's no
// absolute value that means "good enough" across different queries. So
// instead of thresholding on score directly, we rank the distinct score
// values found for the current query and keep only the top
// m_max_score_tiers of them.
void ProductsTreeFilterModel::apply_cutoff_and_coverage(
    const QHash<ProductsModelNode*, double>& scores, const QSet<double>& distinct_scores,
    double max_score)
{
    m_node_scores = scores;
    m_distinct_scores = distinct_scores;
    m_max_score = max_score;
    m_score_cutoff = 0;

    for (auto it = m_coverage.begin(); it != m_coverage.end(); ++it)
        it.value().matched = 0;

    if (distinct_scores.isEmpty())
        return;

    QList<double> sorted_scores = distinct_scores.values();
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<double>());
    int tier_index = std::min(m_max_score_tiers, static_cast<int>(sorted_scores.size())) - 1;
    m_score_cutoff = sorted_scores[tier_index];

    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
    {
        if (it.value() < m_score_cutoff)
            continue;
        for (auto* ancestor = it.key()->parent_node(); ancestor != nullptr;
             ancestor = ancestor->parent_node())
            m_coverage[ancestor].matched += 1;
    }
}

bool ProductsTreeFilterModel::filters_match(ProductsModelNode* node, const Query& query) const
{
    for (const auto& filter : query.filters)
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

// Path and metadata are scored separately and the max is kept per token,
// instead of concatenating them into one candidate string -- otherwise a
// node with rich metadata (e.g. CDAWeb's verbose ISTP-style attributes)
// gets its length penalty inflated by text that has nothing to do with
// whether the path itself is a clean match.
int ProductsTreeFilterModel::free_text_score(ProductsModelNode* node, const Query& query) const
{
    if (query.free_text_tokens.isEmpty())
        return 1;

    QString path_text = node->path().join(' ');
    const QString& meta_text = node->raw_text();
    int total = 0;
    for (const auto& token : query.free_text_tokens)
    {
        int s = std::max(subsequence_score(token, path_text), subsequence_score(token, meta_text));
        if (s == 0)
            return 0;
        total += s;
    }
    return total;
}
