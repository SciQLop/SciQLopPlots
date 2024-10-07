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
#include "Products/ProductsNode.hpp"

ProductsModelNode* ProductsModelNode::_root_node()
{
    ProductsModelNode* node = this;
    while (node->parent_node() != nullptr)
        node = node->parent_node();
    return node;
}

ProductsModelNode::ProductsModelNode(const QString& name, const QMap<QString, QVariant>& metadata,
                                     ProductsModelNodeType m_node_type, QObject* parent)
        : QObject(parent), m_node_type(m_node_type), m_metadata(metadata)
{
    this->setObjectName(name);
    m_tooltip.append(name + "\n");
    for (auto [key, value] : metadata.asKeyValueRange())
    {
        m_tooltip.append(key + " : " + value.toString() + "\n");
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

void ProductsModelNode::set_icon(const QIcon& icon)
{
    m_icon = icon;
}

QIcon ProductsModelNode::icon()
{
    return m_icon;
}

void ProductsModelNode::set_tooltip(const QString& tooltip)
{
    m_tooltip = tooltip;
}

QString ProductsModelNode::tooltip()
{
    return m_tooltip;
}
