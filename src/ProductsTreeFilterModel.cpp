#include "SciQLopPlots/Products/ProductsTreeFilterModel.hpp"
#include "SciQLopPlots/Products/ProductsModel.hpp"
#include "SciQLopPlots/Products/SubsequenceMatcher.hpp"
#include <QDateTime>
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
    m_query = query;
    beginFilterChange();
    endFilterChange();
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

    return node_matches(node);
}

bool ProductsTreeFilterModel::node_matches(ProductsModelNode* node) const
{
    return filters_match(node) && free_text_score(node) > 0;
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

    int total = 0;
    for (const auto& token : m_query.free_text_tokens)
    {
        int s = subsequence_score(token, node->raw_text());
        if (s == 0)
            return 0;
        total += s;
    }
    return total;
}
