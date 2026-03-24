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
    endResetModel();

    m_pending_leaves.clear();
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

void ProductsFlatFilterModel::collect_all_leaves(ProductsModelNode* node,
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

void ProductsFlatFilterModel::process_batch()
{
    int generation = m_batch_generation;
    int end = std::min(m_batch_cursor + BATCH_SIZE, static_cast<int>(m_pending_leaves.size()));

    QList<ScoredNode> batch_results;
    for (int i = m_batch_cursor; i < end; ++i)
    {
        if (m_batch_generation != generation)
            return;

        auto* node = m_pending_leaves[i];
        if (!filters_match(node))
            continue;
        int score = free_text_score(node);
        if (score > 0)
            batch_results.append({ node, score });
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
    {
        m_batch_timer->stop();

        // Sort by score descending — emit layoutChanged so views update order
        emit layoutAboutToBeChanged();
        std::sort(m_results.begin(), m_results.end(),
                  [](const ScoredNode& a, const ScoredNode& b) { return a.score > b.score; });
        emit layoutChanged();
    }
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

int ProductsFlatFilterModel::free_text_score(ProductsModelNode* node) const
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
