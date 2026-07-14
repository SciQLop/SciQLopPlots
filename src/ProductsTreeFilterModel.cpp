#include "SciQLopPlots/Products/ProductsTreeFilterModel.hpp"
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include "SciQLopPlots/Products/SubsequenceMatcher.hpp"
#include <QDateTime>
#include <QSet>
#include <algorithm>
#include <magic_enum/magic_enum.hpp>

ProductsTreeFilterModel::ProductsTreeFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void ProductsTreeFilterModel::set_query(const Query& query)
{
    // Contract: beginFilterChange() snapshots the old mapping and must run
    // BEFORE the filter parameter changes (Qt 6.11 docs).
    beginFilterChange();
    m_query = query;
    recompute_score_cutoff();
    endFilterChange();
}

void ProductsTreeFilterModel::set_max_score_tiers(int max_tiers)
{
    max_tiers = std::max(1, max_tiers);
    if (max_tiers == m_max_score_tiers)
        return;

    beginFilterChange();
    m_max_score_tiers = max_tiers;
    recompute_score_cutoff();
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
    recompute_score_cutoff();
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

    return node_matches(node);
}

bool ProductsTreeFilterModel::node_matches(ProductsModelNode* node) const
{
    if (!filters_match(node))
        return false;
    int score = free_text_score(node);
    return score > 0 && score >= m_score_cutoff;
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
        int score = free_text_score(node);
        if (score <= 0)
            return {};
        return qRound(score * 100.0 / m_max_score);
    }
    if (role == ProductsCoverageRole)
    {
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

// Raw free-text scores are a small, coarse, query/corpus-dependent scale
// (see free_text_score's length-penalty normalization) — there's no
// absolute value that means "good enough" across different queries. So
// instead of thresholding on score directly, we rank the distinct score
// values found for the current query and keep only the top
// m_max_score_tiers of them. This is recomputed whenever the query, the
// tier count, or the source tree itself changes.
void ProductsTreeFilterModel::recompute_score_cutoff()
{
    m_score_cutoff = 0;
    m_max_score = 0;

    for (auto it = m_coverage.begin(); it != m_coverage.end(); ++it)
        it.value().matched = 0;

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

    QSet<int> distinct_scores;
    for (auto* node : leaves)
    {
        if (!filters_match(node))
            continue;
        int score = free_text_score(node);
        if (score > 0)
        {
            distinct_scores.insert(score);
            m_max_score = std::max(m_max_score, score);
        }
    }

    if (distinct_scores.isEmpty())
        return;

    QList<int> sorted_scores = distinct_scores.values();
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<int>());
    int tier_index = std::min(m_max_score_tiers, static_cast<int>(sorted_scores.size())) - 1;
    m_score_cutoff = sorted_scores[tier_index];

    for (auto* node : leaves)
    {
        if (!node_matches(node))
            continue;
        for (auto* ancestor = node->parent_node(); ancestor != nullptr;
             ancestor = ancestor->parent_node())
            m_coverage[ancestor].matched += 1;
    }
}

bool ProductsTreeFilterModel::filters_match(ProductsModelNode* node) const
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

int ProductsTreeFilterModel::free_text_score(ProductsModelNode* node) const
{
    if (m_query.free_text_tokens.isEmpty())
        return 1;

    QString full_text = node->path().join(' ') + ' ' + node->raw_text();
    int total = 0;
    for (const auto& token : m_query.free_text_tokens)
    {
        int s = subsequence_score(token, full_text);
        if (s == 0)
            return 0;
        total += s;
    }
    return total;
}
