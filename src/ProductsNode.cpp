/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include "SciQLopPlots/Products/ProductsNode.hpp"
#include "SciQLopPlots/Icons/icons.hpp"
#include "fmt/core.h"

ProductsModelNode* ProductsModelNode::_root_node()
{
    ProductsModelNode* node = this;
    while (node->parent_node() != nullptr)
        node = node->parent_node();
    return node;
}

ProductsModelNode::ProductsModelNode(const QString& name, const QMap<QString, QVariant>& metadata,
                                     const QString& icon, QObject* parent)
        : ProductsModelNode(name, "", metadata, ProductsModelNodeType::FOLDER,
                            ParameterType::NotAParameter, icon, parent)
{
}

ProductsModelNode::ProductsModelNode(const QString& name, const QString& provider,
                                     const QMap<QString, QVariant>& metadata,
                                     ProductsModelNodeType node_type, ParameterType parameter_type,
                                     const QString& icon, QObject* parent)
        : QObject(parent)
        , m_node_type(node_type)
        , m_metadata(metadata)
        , m_icon(icon)
        , m_parameter_type(parameter_type)
        , m_provider(provider)
{
    this->setObjectName(name);
    m_tooltip.append(fmt::format("<h3>{}</h3>", name.toStdString()).c_str());
    m_raw_text.append(name);
    for (auto [key, value] : metadata.asKeyValueRange())
    {
        m_tooltip.append(
            fmt::format("<br/><b>{}:</b> {}", key.toStdString(), value.toString().toStdString())
                .c_str());
        m_raw_text.append(key + ": " + value.toString());
    }
}

void ProductsModelNode::add_child(ProductsModelNode* child)
{
    for (auto node : m_children)
    {
        if (node->name() == child->name())
        {
            m_children.removeOne(node);
        }
    }
    m_children.append(child);
    child->setParent(this);
}

QStringList ProductsModelNode::path()
{
    QStringList path;
    ProductsModelNode* node = this;
    while (node != nullptr)
    {
        path.prepend(node->objectName());
        node = node->parent_node();
    }
    return path;
}

void ProductsModelNode::set_icon(const QString& name)
{
    m_icon = name;
}

const QIcon& ProductsModelNode::icon()
{
    return Icons::get_icon(m_icon);
}

void ProductsModelNode::set_tooltip(const QString& tooltip)
{
    m_tooltip = tooltip;
}

QStringList ProductsModelNode::completions() const noexcept
{
    QStringList completions;
    completions.append(this->name());
    for (auto [key, value] : m_metadata.asKeyValueRange())
    {
        if (auto v = value.toString(); v.size() < 100)
            completions.append(key + ": " + v);
    }
    return completions;
}
