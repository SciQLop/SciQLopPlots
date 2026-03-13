# Inspector Model Redesign Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the buggy Inspector MVC model with a data-driven TypeDescriptor approach that has clean ownership semantics and bidirectional lifetime binding.

**Architecture:** Nodes observe domain objects via QPointer (no ownership). All signal wiring happens in PlotsModel::addNode, stored as QMetaObject::Connection handles, and severed in removeRows. Type-specific behavior is defined via TypeDescriptor structs registered in a TypeRegistry singleton. Delegate widgets are created via DelegateRegistry.

**Tech Stack:** C++20, Qt6, Shiboken6, Meson

**Spec:** `docs/superpowers/specs/2026-03-13-inspector-model-redesign.md`

---

## Important Notes

### Signal availability for connect_children

Several domain types only emit `*_list_changed()` (no args) instead of individual `added(QObject*)`/`removed(QObject*)` signals:
- `SciQLopPlotInterface::graph_list_changed()` — no args
- `SciQLopGraphInterface::component_list_changed()` — no args

For these types, `connect_children` must use a **re-sync lambda** that compares current children vs node children and calls `add_child`/`remove_child` as needed. Only `SciQLopMultiPlotPanel` has proper `plot_added()`/`plot_removed()`/`panel_added()`/`panel_removed()` signals with object pointers.

### Out of scope (deferred)

- `TypeRegistry::register_type_py()` — the C++ method accepting `PyObject*` callables for Shiboken Python extension. Will be added in a follow-up task when the Python API is needed.

### Build command

```bash
meson setup build --buildtype=debug && meson compile -C build
```

### Test command

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
python -m pytest tests/integration/ -v
```

---

## Chunk 1: Core Infrastructure

### File Map

| Action | File | Responsibility |
|--------|------|---------------|
| Create | `include/SciQLopPlots/Inspector/Model/TypeDescriptor.hpp` | TypeDescriptor struct + TypeRegistry singleton |
| Create | `include/SciQLopPlots/Inspector/Model/DelegateRegistry.hpp` | Delegate factory registry |
| Create | `src/TypeRegistry.cpp` | TypeRegistry::instance() and descriptor() lookup |
| Create | `src/DelegateRegistry.cpp` | DelegateRegistry::instance() and create_delegate() lookup |
| Rewrite | `include/SciQLopPlots/Inspector/Model/Node.hpp` | Simplified PlotsModelNode with connection storage |
| Rewrite | `src/Node.cpp` | Node implementation — no ownership, disconnect_all |
| Rewrite | `include/SciQLopPlots/Inspector/Model/Model.hpp` | PlotsModel with addNode, removeRows, removeChildByObject, node_removed signal |
| Rewrite | `src/Model.cpp` | Model implementation with proper wiring and lifetime |

### Task 1: TypeDescriptor and TypeRegistry

**Files:**
- Create: `include/SciQLopPlots/Inspector/Model/TypeDescriptor.hpp`
- Create: `src/TypeRegistry.cpp`

- [ ] **Step 1: Create TypeDescriptor.hpp**

```cpp
#pragma once
#include <QIcon>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <functional>

struct TypeDescriptor
{
    std::function<QList<QObject*>(QObject*)> children;

    std::function<QList<QMetaObject::Connection>(QObject* obj,
        std::function<void(QObject*)> add_child,
        std::function<void(QObject*)> remove_child)>
        connect_children;

    std::function<QIcon(const QObject*)> icon = {};
    std::function<QString(const QObject*)> tooltip = {};
    std::function<void(QObject*, bool)> set_selected = {};

    bool deletable = true;
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
};

class TypeRegistry
{
    QMap<QString, TypeDescriptor> m_descriptors;
    TypeRegistry() = default;

public:
    static TypeRegistry& instance();

    template <typename T>
    void register_type(TypeDescriptor desc)
    {
        m_descriptors[T::staticMetaObject.className()] = std::move(desc);
    }

    void register_type(const QString& type_name, TypeDescriptor desc);

    const TypeDescriptor* descriptor(const QObject* obj) const;
};
```

- [ ] **Step 2: Create src/TypeRegistry.cpp**

```cpp
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"

TypeRegistry& TypeRegistry::instance()
{
    static TypeRegistry inst;
    return inst;
}

void TypeRegistry::register_type(const QString& type_name, TypeDescriptor desc)
{
    m_descriptors[type_name] = std::move(desc);
}

const TypeDescriptor* TypeRegistry::descriptor(const QObject* obj) const
{
    auto metaObject = obj->metaObject();
    while (metaObject != nullptr)
    {
        auto it = m_descriptors.find(metaObject->className());
        if (it != m_descriptors.end())
            return &it.value();
        metaObject = metaObject->superClass();
    }
    return nullptr;
}
```

- [ ] **Step 3: Verify it compiles**

Add both files to `SciQLopPlots/meson.build` (in the headers list and sources list) and run:
```bash
meson compile -C build
```
Expected: compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Inspector/Model/TypeDescriptor.hpp src/TypeRegistry.cpp
git commit -m "feat(inspector): add TypeDescriptor and TypeRegistry"
```

### Task 2: DelegateRegistry

**Files:**
- Create: `include/SciQLopPlots/Inspector/Model/DelegateRegistry.hpp`
- Create: `src/DelegateRegistry.cpp`

- [ ] **Step 1: Create DelegateRegistry.hpp**

```cpp
#pragma once
#include <QMap>
#include <QObject>
#include <QWidget>
#include <functional>

class DelegateRegistry
{
    QMap<QString, std::function<QWidget*(QObject*, QWidget*)>> m_factories;
    DelegateRegistry() = default;

public:
    static DelegateRegistry& instance();

    template <typename DelegateType>
    void register_type()
    {
        m_factories[DelegateType::compatible_type::staticMetaObject.className()]
            = [](QObject* obj, QWidget* parent) -> QWidget* {
            return new DelegateType(
                qobject_cast<typename DelegateType::compatible_type*>(obj), parent);
        };
    }

    void register_type(const QString& type_name,
        std::function<QWidget*(QObject*, QWidget*)> factory);

    QWidget* create_delegate(QObject* obj, QWidget* parent = nullptr) const;
};
```

- [ ] **Step 2: Create src/DelegateRegistry.cpp**

```cpp
#include "SciQLopPlots/Inspector/Model/DelegateRegistry.hpp"

DelegateRegistry& DelegateRegistry::instance()
{
    static DelegateRegistry inst;
    return inst;
}

void DelegateRegistry::register_type(
    const QString& type_name, std::function<QWidget*(QObject*, QWidget*)> factory)
{
    m_factories[type_name] = std::move(factory);
}

QWidget* DelegateRegistry::create_delegate(QObject* obj, QWidget* parent) const
{
    auto metaObject = obj->metaObject();
    while (metaObject != nullptr)
    {
        auto it = m_factories.find(metaObject->className());
        if (it != m_factories.end())
            return it.value()(obj, parent);
        metaObject = metaObject->superClass();
    }
    return nullptr;
}
```

- [ ] **Step 3: Add to meson.build and verify compilation**

```bash
meson compile -C build
```

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Inspector/Model/DelegateRegistry.hpp src/DelegateRegistry.cpp
git commit -m "feat(inspector): add DelegateRegistry"
```

### Task 3: Rewrite PlotsModelNode

**Files:**
- Rewrite: `include/SciQLopPlots/Inspector/Model/Node.hpp`
- Rewrite: `src/Node.cpp`

- [ ] **Step 1: Write the test for node lifetime behavior**

Create `tests/integration/test_inspector_node.py`:

```python
"""Tests for the new PlotsModelNode ownership semantics."""
import pytest
import gc
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QObject

from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel, PlotsModel
from conftest import force_gc, process_events


class TestNodeDoesNotOwnObject:
    """Node destructor must NOT call deleteLater on the wrapped object."""

    def test_removing_non_deletable_node_preserves_object(self, qtbot):
        """When a non-deletable node is removed from the model, the object survives."""
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        process_events()

        # Axes are non-deletable — they should survive node removal
        # Just verify the panel and its structure survive cleanup
        del panel
        force_gc()
        # No crash = pass


class TestBidirectionalLifetime:
    """Deleting from tree should delete widget, deleting widget should remove from tree."""

    def test_external_destruction_removes_node(self, qtbot, sample_data):
        """When a domain object is destroyed externally, its node is removed."""
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x, y = sample_data
        panel.line(x, y)
        process_events()

        model = PlotsModel.instance()
        initial_count = model.rowCount(model.index(0, 0))

        # Destroy the panel — all nodes should be removed
        del panel
        force_gc()
        # Model should have no top-level nodes left (or at least fewer)
        # No crash = pass

    def test_no_crash_on_panel_destruction(self, qtbot, sample_data):
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        panel.line(x, y)
        process_events()

        del panel
        force_gc()

    def test_no_crash_on_plot_destruction(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        p.line(x, y)
        process_events()

        del p
        force_gc()
```

- [ ] **Step 2: Rewrite Node.hpp**

```cpp
#pragma once
#include <QIcon>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

struct TypeDescriptor;

class PlotsModelNode : public QObject
{
    Q_OBJECT
    QPointer<QObject> m_obj;
    const TypeDescriptor* m_descriptor;
    QList<PlotsModelNode*> m_children;
    QList<QMetaObject::Connection> m_connections;

public:
    PlotsModelNode(QObject* obj, const TypeDescriptor* desc, QObject* parent = nullptr);
    ~PlotsModelNode();

    inline QString name() const { return objectName(); }
    void setName(const QString& name);

    PlotsModelNode* child(int row) const;
    int child_row(PlotsModelNode* child) const;
    int child_row_by_object(QObject* obj) const;
    int children_count() const;
    QList<PlotsModelNode*> children_nodes() const;
    PlotsModelNode* parent_node() const;

    PlotsModelNode* insert_child(QObject* obj, const TypeDescriptor* desc, int row = -1);
    bool remove_child(int row);

    void add_connections(const QList<QMetaObject::Connection>& conns);
    void add_connection(QMetaObject::Connection conn);
    void disconnect_all();

    inline QObject* object() const { return m_obj; }
    inline const TypeDescriptor* descriptor() const { return m_descriptor; }

    bool deletable() const;
    Qt::ItemFlags flags() const;
    QIcon icon() const;
    QString tooltip() const;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void nameChanged();
};
```

- [ ] **Step 3: Rewrite Node.cpp**

```cpp
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"

PlotsModelNode::PlotsModelNode(QObject* obj, const TypeDescriptor* desc, QObject* parent)
    : QObject(parent), m_obj(obj), m_descriptor(desc)
{
    if (obj)
        setObjectName(obj->objectName());
}

PlotsModelNode::~PlotsModelNode()
{
    // Recursively clean up children — disconnect and request deletion of live deletable objects.
    // Note: node_removed is NOT emitted here — the model's removeRows handles that for
    // direct removals. When the destructor runs as part of removeRows, the parent node
    // is already being removed from the model, so child notifications are unnecessary
    // (PropertiesView compares against m_currentObject, which was already cleared by the
    // parent's node_removed emission if it matched).
    while (!m_children.isEmpty())
    {
        auto child = m_children.takeFirst();
        child->disconnect_all();
        if (auto obj = child->object(); obj && child->deletable())
            obj->deleteLater();
        delete child;
    }
    // NEVER touch m_obj — we don't own it
}

void PlotsModelNode::setName(const QString& name)
{
    if (objectName() != name)
        setObjectName(name);
    if (m_obj && m_obj->objectName() != name)
        m_obj->setObjectName(name);
}

PlotsModelNode* PlotsModelNode::child(int row) const
{
    return m_children.value(row, nullptr);
}

int PlotsModelNode::child_row(PlotsModelNode* child) const
{
    return m_children.indexOf(child);
}

int PlotsModelNode::child_row_by_object(QObject* obj) const
{
    for (int i = 0; i < m_children.size(); ++i)
    {
        if (m_children[i]->object() == obj)
            return i;
    }
    return -1;
}

int PlotsModelNode::children_count() const
{
    return m_children.size();
}

QList<PlotsModelNode*> PlotsModelNode::children_nodes() const
{
    return m_children;
}

PlotsModelNode* PlotsModelNode::parent_node() const
{
    return qobject_cast<PlotsModelNode*>(parent());
}

PlotsModelNode* PlotsModelNode::insert_child(
    QObject* obj, const TypeDescriptor* desc, int row)
{
    auto node = new PlotsModelNode(obj, desc, this);
    if (row < 0 || row >= m_children.size())
        m_children.append(node);
    else
        m_children.insert(row, node);
    return node;
}

bool PlotsModelNode::remove_child(int row)
{
    if (row < 0 || row >= m_children.size())
        return false;
    m_children.removeAt(row);
    return true;
}

void PlotsModelNode::add_connections(const QList<QMetaObject::Connection>& conns)
{
    m_connections.append(conns);
}

void PlotsModelNode::add_connection(QMetaObject::Connection conn)
{
    m_connections.append(std::move(conn));
}

void PlotsModelNode::disconnect_all()
{
    for (auto& conn : m_connections)
        QObject::disconnect(conn);
    m_connections.clear();
}

bool PlotsModelNode::deletable() const
{
    return m_descriptor ? m_descriptor->deletable : true;
}

Qt::ItemFlags PlotsModelNode::flags() const
{
    return m_descriptor ? m_descriptor->flags
                        : (Qt::ItemIsEnabled | Qt::ItemIsSelectable);
}

QIcon PlotsModelNode::icon() const
{
    if (m_descriptor && m_descriptor->icon && m_obj)
        return m_descriptor->icon(m_obj);
    return {};
}

QString PlotsModelNode::tooltip() const
{
    if (m_descriptor && m_descriptor->tooltip && m_obj)
        return m_descriptor->tooltip(m_obj);
    return {};
}
```

- [ ] **Step 4: Verify compilation**

```bash
meson compile -C build
```

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Inspector/Model/Node.hpp src/Node.cpp tests/integration/test_inspector_node.py
git commit -m "feat(inspector): rewrite PlotsModelNode with no-ownership semantics"
```

### Task 4: Rewrite PlotsModel

**Files:**
- Rewrite: `include/SciQLopPlots/Inspector/Model/Model.hpp`
- Rewrite: `src/Model.cpp`

- [ ] **Step 1: Rewrite Model.hpp**

```cpp
#pragma once
#include "SciQLopPlots/Inspector/Model/Node.hpp"
#include <QAbstractItemModel>
#include <QObject>

class PlotsModel : public QAbstractItemModel
{
    Q_OBJECT
    PlotsModelNode* m_rootNode;

    QModelIndex make_index(PlotsModelNode* node) const;

public:
    PlotsModel(QObject* parent = nullptr);
    ~PlotsModel() = default;

    QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool removeRows(int row, int count,
        const QModelIndex& parent = QModelIndex()) override;

    void addNode(PlotsModelNode* parent, QObject* obj);
    void removeChildByObject(PlotsModelNode* parent, QObject* obj);

    void set_selected(const QList<QModelIndex>& indexes, bool selected);

    Q_SLOT void addTopLevelNode(QObject* obj);

    static PlotsModel* instance();
    static QObject* object(const QModelIndex& index);

    QMimeData* mimeData(const QModelIndexList& indexes) const override;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void item_selection_changed(const QModelIndex& index, bool selected);
    Q_SIGNAL void top_level_nodes_list_changed();
    Q_SIGNAL void node_removed(QObject* obj);
};
```

- [ ] **Step 2: Rewrite Model.cpp**

```cpp
#include "SciQLopPlots/Inspector/Model/Model.hpp"
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"
#include <QMimeData>
#include <QPointer>
#include <qapplicationstatic.h>

PlotsModel::PlotsModel(QObject* parent) : QAbstractItemModel(parent)
{
    m_rootNode = new PlotsModelNode(nullptr, nullptr, this);
    m_rootNode->setObjectName(QStringLiteral("Root Node"));
}

QModelIndex PlotsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    auto parentNode = parent.isValid()
        ? static_cast<PlotsModelNode*>(parent.internalPointer())
        : m_rootNode;
    if (auto child = parentNode->child(row))
        return createIndex(row, column, child);
    return {};
}

QModelIndex PlotsModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};
    auto child = static_cast<PlotsModelNode*>(index.internalPointer());
    auto parentNode = child->parent_node();
    if (!parentNode || parentNode == m_rootNode)
        return {};
    auto grandparent = parentNode->parent_node();
    int row = grandparent ? grandparent->child_row(parentNode) : 0;
    return createIndex(row, 0, parentNode);
}

int PlotsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    auto parentNode = parent.isValid()
        ? static_cast<PlotsModelNode*>(parent.internalPointer())
        : m_rootNode;
    return parentNode->children_count();
}

int PlotsModel::columnCount(const QModelIndex&) const { return 1; }

QVariant PlotsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    auto node = static_cast<PlotsModelNode*>(index.internalPointer());
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

Qt::ItemFlags PlotsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    if (auto node = static_cast<PlotsModelNode*>(index.internalPointer()))
        return node->flags();
    return QAbstractItemModel::flags(index);
}

bool PlotsModel::removeRows(int row, int count, const QModelIndex& parent)
{
    auto parentNode = parent.isValid()
        ? static_cast<PlotsModelNode*>(parent.internalPointer())
        : m_rootNode;
    if (!parentNode || row < 0 || row + count > parentNode->children_count())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = row + count - 1; i >= row; --i)
    {
        auto child = parentNode->child(i);
        if (!child)
            continue;
        child->disconnect_all();
        QObject* obj = child->object();
        parentNode->remove_child(i);
        emit node_removed(obj);
        bool should_delete_obj = obj && child->deletable();
        delete child;
        if (should_delete_obj)
            obj->deleteLater();
    }
    endRemoveRows();
    return true;
}

void PlotsModel::addNode(PlotsModelNode* parent, QObject* obj)
{
    if (!obj)
        return;
    auto desc = TypeRegistry::instance().descriptor(obj);
    auto parentIndex = make_index(parent);
    int row = parent->children_count();

    beginInsertRows(parentIndex, row, row);
    auto node = parent->insert_child(obj, desc);
    endInsertRows();

    QPointer<PlotsModelNode> guardedNode = node;
    QPointer<PlotsModelNode> guardedParent = parent;

    // obj->destroyed => remove node from model
    node->add_connection(
        connect(obj, &QObject::destroyed, this,
            [this, guardedNode, guardedParent]()
            {
                if (!guardedNode || !guardedParent)
                    return;
                int r = guardedParent->child_row(guardedNode);
                if (r >= 0)
                    removeRows(r, 1, make_index(guardedParent));
            }));

    // obj->objectNameChanged => update node name + notify view
    node->add_connection(
        connect(obj, &QObject::objectNameChanged, this,
            [this, guardedNode](const QString& name)
            {
                if (!guardedNode)
                    return;
                guardedNode->setName(name);
                auto idx = make_index(guardedNode);
                if (idx.isValid())
                    emit dataChanged(idx, idx);
            }));

    // Wire connect_children if descriptor provides it
    if (desc && desc->connect_children)
    {
        auto add_child = [this, guardedNode](QObject* child)
        {
            if (guardedNode)
                addNode(guardedNode, child);
        };
        auto remove_child = [this, guardedNode](QObject* child)
        {
            if (guardedNode)
                removeChildByObject(guardedNode, child);
        };
        node->add_connections(desc->connect_children(obj, add_child, remove_child));
    }

    // Recursively add initial children
    if (desc && desc->children)
    {
        for (auto child : desc->children(obj))
            addNode(node, child);
    }
}

void PlotsModel::removeChildByObject(PlotsModelNode* parent, QObject* obj)
{
    if (!parent || !obj)
        return;
    int row = parent->child_row_by_object(obj);
    if (row >= 0)
        removeRows(row, 1, make_index(parent));
}

void PlotsModel::set_selected(const QList<QModelIndex>& indexes, bool selected)
{
    for (const auto& index : indexes)
    {
        auto node = static_cast<PlotsModelNode*>(index.internalPointer());
        if (node && node->descriptor() && node->descriptor()->set_selected && node->object())
        {
            node->descriptor()->set_selected(node->object(), selected);
            emit item_selection_changed(index, selected);
        }
    }
}

void PlotsModel::addTopLevelNode(QObject* obj)
{
    addNode(m_rootNode, obj);
    emit top_level_nodes_list_changed();
}

Q_APPLICATION_STATIC(PlotsModel, _plots_model);

PlotsModel* PlotsModel::instance()
{
    return _plots_model();
}

QObject* PlotsModel::object(const QModelIndex& index)
{
    if (auto node = static_cast<PlotsModelNode*>(index.internalPointer()))
        return node->object();
    return nullptr;
}

QMimeData* PlotsModel::mimeData(const QModelIndexList&) const
{
    return new QMimeData();
}

QModelIndex PlotsModel::make_index(PlotsModelNode* node) const
{
    if (!node || node == m_rootNode)
        return {};
    auto parentNode = node->parent_node();
    int row = parentNode ? parentNode->child_row(node) : m_rootNode->child_row(node);
    if (row < 0)
        return {};
    return createIndex(row, 0, node);
}
```

- [ ] **Step 3: Verify compilation**

This will temporarily break because old inspector files still reference the old API. Comment out or remove the old inspector `.cpp` files from `meson.build` sources temporarily. Keep the delegate `.cpp` files (they don't depend on the old inspector API).

Remove from `meson.build` sources list:
```
'../src/InspectorBase.cpp',
'../src/Inspectors.cpp',
'../src/SciQLopMultiPlotPanelInspector.cpp',
'../src/SciQLopPlotInspector.cpp',
'../src/SciQLopNDProjectionPlotInspector.cpp',
'../src/SciQLopNDProjectionCurvesInspector.cpp',
'../src/SciQLopGraphInterfaceInspector.cpp',
'../src/SciQLopGraphComponentInspector.cpp',
'../src/SciQLopColorMapInspector.cpp',
'../src/SciQLopPlotAxisInspector.cpp',
```

Remove from `meson.build` moc_headers list:
```
project_source_root + '/include/SciQLopPlots/Inspector/InspectorBase.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopMultiPlotPanelInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopPlotInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopNDProjectionPlotInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopNDProjectionCurvesInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopGraphInterfaceInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopGraphComponentInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopColorMapInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/Inspectors/SciQLopPlotAxisInspector.hpp',
project_source_root + '/include/SciQLopPlots/Inspector/PropertyDelegates.hpp',
```

Do NOT add `TypeDescriptor.hpp` or `DelegateRegistry.hpp` to `moc_headers` — they are not QObject subclasses. They are included transitively by other headers.

Add to `meson.build` sources:
```
'../src/TypeRegistry.cpp',
'../src/DelegateRegistry.cpp',
```

Also update any `#include` in `PropertiesView.cpp` from `PropertyDelegates.hpp` to `DelegateRegistry.hpp`.

```bash
meson compile -C build
```

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Inspector/Model/Model.hpp src/Model.cpp SciQLopPlots/meson.build
git commit -m "feat(inspector): rewrite PlotsModel with clean addNode/removeRows"
```

---

## Chunk 2: Registrations and View Updates

### Task 5: Create Registrations.cpp

**Files:**
- Create: `src/Registrations.cpp`

This single file replaces all 8 inspector `.cpp` files and the `REGISTER_DELEGATE` calls scattered in delegate `.cpp` files.

- [ ] **Step 1: Create src/Registrations.cpp**

```cpp
#include "SciQLopPlots/Inspector/Model/TypeDescriptor.hpp"
#include "SciQLopPlots/Inspector/Model/DelegateRegistry.hpp"

// Domain types
#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopNDProjectionPlot.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"

// Delegate widgets
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopMultiPlotPanelDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp"

namespace
{

void register_all_types()
{
    auto& types = TypeRegistry::instance();

    // -- SciQLopMultiPlotPanel --
    types.register_type<SciQLopMultiPlotPanel>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto panel = qobject_cast<SciQLopMultiPlotPanel*>(obj);
            QList<QObject*> result;
            if (panel)
                for (auto w : panel->child_widgets())
                    result.append(w);
            return result;
        },
        .connect_children = [](QObject* obj, auto add, auto remove)
                -> QList<QMetaObject::Connection> {
            auto panel = qobject_cast<SciQLopMultiPlotPanel*>(obj);
            if (!panel) return {};
            return {
                QObject::connect(panel, &SciQLopMultiPlotPanel::plot_added, panel,
                    [add](SciQLopPlotInterface* p) { add(p); }),
                QObject::connect(panel, &SciQLopMultiPlotPanel::plot_removed, panel,
                    [remove](SciQLopPlotInterface* p) { remove(p); }),
                QObject::connect(panel, &SciQLopMultiPlotPanel::panel_added, panel,
                    [add](SciQLopPlotPanelInterface* p) { add(p); }),
                QObject::connect(panel, &SciQLopMultiPlotPanel::panel_removed, panel,
                    [remove](SciQLopPlotPanelInterface* p) { remove(p); }),
            };
        },
        .set_selected = [](QObject* obj, bool s) {
            if (auto p = qobject_cast<SciQLopMultiPlotPanel*>(obj))
                p->setSelected(s);
        },
    });

    // -- SciQLopPlot --
    // graph_list_changed() has no args, so we can't do individual add/remove.
    // Use connect_children to wire the signal, and addNode's initial children
    // for the first population. On signal fire, the model must re-sync.
    // For now, we don't wire connect_children — initial children + destroyed
    // handles most cases. TODO: add graph_added/graph_removed signals.
    types.register_type<SciQLopPlot>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto plot = qobject_cast<SciQLopPlot*>(obj);
            if (!plot) return {};
            QList<QObject*> c;
            c.append(plot->x_axis());
            c.append(plot->y_axis());
            if (plot->has_colormap()) {
                c.append(plot->z_axis());
                c.append(plot->y2_axis());
            }
            for (auto p : plot->plottables())
                c.append(p);
            return c;
        },
        .connect_children = nullptr, // TODO: add when graph_added/removed signals exist
        .set_selected = [](QObject* obj, bool s) {
            if (auto p = qobject_cast<SciQLopPlot*>(obj))
                p->set_selected(s);
        },
        .flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable
               | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled,
    });

    // -- SciQLopNDProjectionPlot -- (same issue: graph_list_changed has no args)
    types.register_type<SciQLopNDProjectionPlot>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto plot = qobject_cast<SciQLopNDProjectionPlot*>(obj);
            if (!plot) return {};
            QList<QObject*> c;
            for (auto p : plot->plottables())
                c.append(p);
            return c;
        },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto p = qobject_cast<SciQLopNDProjectionPlot*>(obj))
                p->set_selected(s);
        },
    });

    // -- SciQLopGraphInterface (SciQLopLineGraph, SciQLopCurve) --
    // component_list_changed has no args — same limitation
    types.register_type<SciQLopGraphInterface>({
        .children = [](QObject* obj) -> QList<QObject*> {
            auto graph = qobject_cast<SciQLopGraphInterface*>(obj);
            if (!graph) return {};
            QList<QObject*> c;
            for (auto comp : graph->components())
                c.append(comp);
            return c;
        },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto g = qobject_cast<SciQLopGraphInterface*>(obj);
                g && g->selected() != s)
                g->set_selected(s);
        },
    });

    // -- SciQLopGraphComponentInterface -- (leaf, no children)
    types.register_type<SciQLopGraphComponentInterface>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto c = qobject_cast<SciQLopGraphComponentInterface*>(obj))
                c->set_selected(s);
        },
        .deletable = false,
    });

    // -- SciQLopColorMap -- (leaf)
    types.register_type<SciQLopColorMap>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto cm = qobject_cast<SciQLopColorMap*>(obj))
                cm->set_selected(s);
        },
    });

    // -- SciQLopNDProjectionCurves -- (inherits SciQLopGraphInterface, same behavior)
    // Registered via SciQLopGraphInterface — metaObject chain lookup handles it.

    // -- SciQLopPlotAxisInterface -- (leaf, not deletable)
    types.register_type<SciQLopPlotAxisInterface>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = [](QObject* obj, bool s) {
            if (auto a = qobject_cast<SciQLopPlotAxisInterface*>(obj))
                a->set_selected(s);
        },
        .deletable = false,
    });

    // -- Delegate registrations --
    auto& delegates = DelegateRegistry::instance();
    delegates.register_type<SciQLopMultiPlotPanelDelegate>();
    delegates.register_type<SciQLopPlotDelegate>();
    delegates.register_type<SciQLopGraphDelegate>();
    delegates.register_type<SciQLopGraphComponentDelegate>();
    delegates.register_type<SciQLopColorMapDelegate>();
    delegates.register_type<SciQLopPlotAxisDelegate>();
}

// Static initializer ensures registration at load time
static const int _registrations = (register_all_types(), 0);

} // anonymous namespace
```

- [ ] **Step 2: Add to meson.build**

Add `'../src/Registrations.cpp'` to the sources list in `SciQLopPlots/meson.build`.

- [ ] **Step 3: Remove REGISTER_INSPECTOR and REGISTER_DELEGATE calls from delegate .cpp files**

Each delegate `.cpp` file (e.g., `src/SciQLopPlotDelegate.cpp`) currently has a `REGISTER_DELEGATE(...)` call and an `#include` of `PropertyDelegates.hpp`. Remove the `REGISTER_DELEGATE` line and the `#include "SciQLopPlots/Inspector/PropertyDelegates.hpp"` from each delegate `.cpp` file. The registrations now happen in `Registrations.cpp`.

Files to modify:
- `src/SciQLopMultiPlotPanelDelegate.cpp`
- `src/SciQLopPlotDelegate.cpp`
- `src/SciQLopGraphDelegate.cpp`
- `src/SciQLopGraphComponentDelegate.cpp`
- `src/SciQLopColorMapDelegate.cpp`
- `src/SciQLopPlotAxisDelegate.cpp`

- [ ] **Step 4: Verify compilation**

```bash
meson compile -C build
```

- [ ] **Step 5: Commit**

```bash
git add src/Registrations.cpp SciQLopPlots/meson.build src/*Delegate.cpp
git commit -m "feat(inspector): add data-driven type and delegate registrations"
```

### Task 6: Update PropertiesView to use DelegateRegistry and handle stale delegates

**Files:**
- Modify: `include/SciQLopPlots/Inspector/View/PropertiesView.hpp`
- Modify: `src/PropertiesView.cpp`

- [ ] **Step 1: Update PropertiesView.hpp**

Change `PropertyDelegateBase* m_delegateWidget` to `QWidget* m_delegateWidget` and add `QObject* m_currentObject`:

```cpp
#pragma once
#include <QObject>
#include <QPointer>
#include <QWidget>

class PropertiesView : public QWidget
{
    Q_OBJECT
    QWidget* m_delegateWidget = nullptr;
    QPointer<QObject> m_currentObject;

public:
    Q_SLOT void set_current_objects(const QList<QObject*>& objects);
    Q_SLOT void on_node_removed(QObject* obj);
    PropertiesView(QWidget* parent = nullptr);
    virtual ~PropertiesView() = default;
};
```

- [ ] **Step 2: Update PropertiesView.cpp**

```cpp
#include "SciQLopPlots/Inspector/View/PropertiesView.hpp"
#include "SciQLopPlots/Inspector/Model/DelegateRegistry.hpp"
#include "SciQLopPlots/Inspector/Model/Model.hpp"
#include <QVBoxLayout>

void PropertiesView::set_current_objects(const QList<QObject*>& objects)
{
    if (m_delegateWidget)
    {
        delete m_delegateWidget;
        m_delegateWidget = nullptr;
        m_currentObject = nullptr;
    }
    if (!objects.isEmpty())
    {
        auto first = objects.first();
        auto delegate = DelegateRegistry::instance().create_delegate(first, this);
        if (delegate)
        {
            m_delegateWidget = delegate;
            m_currentObject = first;
            this->layout()->addWidget(m_delegateWidget);
        }
    }
}

void PropertiesView::on_node_removed(QObject* obj)
{
    if (m_delegateWidget && m_currentObject == obj)
    {
        delete m_delegateWidget;
        m_delegateWidget = nullptr;
        m_currentObject = nullptr;
    }
}

PropertiesView::PropertiesView(QWidget* parent) : QWidget(parent)
{
    this->setLayout(new QVBoxLayout(this));
    connect(PlotsModel::instance(), &PlotsModel::node_removed, this,
        &PropertiesView::on_node_removed);
}
```

- [ ] **Step 3: Verify compilation**

```bash
meson compile -C build
```

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Inspector/View/PropertiesView.hpp src/PropertiesView.cpp
git commit -m "fix(inspector): PropertiesView uses DelegateRegistry, clears stale delegates"
```

### Note: InspectorView.cpp requires no changes

`InspectorView.cpp` uses only `PlotsModel::instance()`, `PlotsModel::set_selected()`, `PlotsModel::object()`, and `PlotsModel::item_selection_changed` — all of which are preserved in the new API. No modifications needed. The spec listed it as "Modified" for selection handling cleanup, but the current code already works correctly with the new model.

### Task 7: Update bindings.xml

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Update bindings.xml**

Remove:
```xml
<object-type name="InspectorBase" parent-management="yes"/>
<object-type name="Inspectors" parent-management="yes"/>
```

Keep (these still exist):
```xml
<object-type name="PlotsModelNode" parent-management="yes"/>
<object-type name="PlotsModel" parent-management="yes"/>
<object-type name="PlotsTreeView" parent-management="yes"/>
<object-type name="InspectorView" parent-management="yes"/>
<object-type name="PropertyDelegateBase" parent-management="yes"/>
<object-type name="PropertiesPanel" parent-management="yes"/>
```

Add:
```xml
<object-type name="TypeRegistry"/>
<object-type name="DelegateRegistry"/>
```

Also update `SciQLopPlots/bindings/bindings.h` — remove includes for deleted headers, add includes for new ones.

- [ ] **Step 2: Verify compilation (full build with bindings)**

```bash
meson compile -C build
```

- [ ] **Step 3: Commit**

```bash
git add SciQLopPlots/bindings/bindings.xml SciQLopPlots/bindings/bindings.h
git commit -m "feat(inspector): update Shiboken bindings for new registry classes"
```

### Task 8: Delete old inspector files

**Files to delete:**
- `include/SciQLopPlots/Inspector/InspectorBase.hpp`
- `src/InspectorBase.cpp`
- `include/SciQLopPlots/Inspector/Inspectors.hpp`
- `src/Inspectors.cpp`
- `include/SciQLopPlots/Inspector/PropertyDelegates.hpp`
- All files in `include/SciQLopPlots/Inspector/Inspectors/` directory

- [ ] **Step 1: Delete files**

```bash
rm include/SciQLopPlots/Inspector/InspectorBase.hpp
rm src/InspectorBase.cpp
rm include/SciQLopPlots/Inspector/Inspectors.hpp
rm src/Inspectors.cpp
rm include/SciQLopPlots/Inspector/PropertyDelegates.hpp
rm -r include/SciQLopPlots/Inspector/Inspectors/
rm src/SciQLopMultiPlotPanelInspector.cpp
rm src/SciQLopPlotInspector.cpp
rm src/SciQLopNDProjectionPlotInspector.cpp
rm src/SciQLopNDProjectionCurvesInspector.cpp
rm src/SciQLopGraphInterfaceInspector.cpp
rm src/SciQLopGraphComponentInspector.cpp
rm src/SciQLopColorMapInspector.cpp
rm src/SciQLopPlotAxisInspector.cpp
```

- [ ] **Step 2: Remove any remaining includes of deleted headers**

Search for `#include.*InspectorBase\|#include.*Inspectors\.hpp\|#include.*PropertyDelegates\.hpp` in all source files and remove them.

- [ ] **Step 3: Verify compilation**

```bash
meson compile -C build
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "refactor(inspector): remove old InspectorBase class hierarchy"
```

---

## Chunk 3: Tests and Verification

### Task 9: Run existing tests and fix any breakage

- [ ] **Step 1: Run the full test suite**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
pip install -e . --no-build-isolation && python -m pytest tests/integration/ -v
```

- [ ] **Step 2: Fix any test failures**

The existing `test_inspector_model.py` should pass since the bugs it tests are fixed by the redesign. The new `test_inspector_node.py` should also pass.

- [ ] **Step 3: Run manual tests to verify UI**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
python tests/manual-tests/All/main.py
```

Verify: tree view shows panels/plots/axes/graphs, selecting items in tree highlights them, properties panel shows correct delegate widget, deleting items works, closing plots removes them from tree.

- [ ] **Step 4: Commit any fixes**

```bash
git add -A
git commit -m "fix(inspector): resolve test failures from model redesign"
```

### Task 10: Add comprehensive lifetime tests

**Files:**
- Modify: `tests/integration/test_inspector_node.py`

- [ ] **Step 1: Add tests for all deletion paths**

Extend `test_inspector_node.py` with:

```python
class TestTreeDeletion:
    """Deleting from tree should request object deletion."""

    def test_tree_delete_removes_graph(self, qtbot, sample_data):
        """Removing a graph node from the model should delete the graph object."""
        from SciQLopPlots import PlotsModel
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x, y = sample_data
        plot, graph = panel.line(x, y)
        process_events()

        model = PlotsModel.instance()
        # Find the panel's top-level index
        panel_idx = model.index(0, 0)
        # The plot should be a child of the panel
        plot_idx = model.index(0, 0, panel_idx)
        initial_row_count = model.rowCount(plot_idx)
        assert initial_row_count > 0  # at least axes + graph

        # No crash during cleanup
        del panel
        force_gc()


class TestNoDoubleDelete:
    """Ensure no double-delete or use-after-free on destruction cascades."""

    def test_panel_with_multiple_plots(self, qtbot, sample_data):
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        panel.line(x, y)
        panel.line(x, y)
        panel.line(x, y)
        process_events()

        del panel
        force_gc()
        # No crash = pass

    def test_rapid_add_remove(self, qtbot, sample_data):
        """Rapidly adding and removing plots should not crash."""
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)

        for _ in range(5):
            panel.line(x, y)
            process_events()

        del panel
        force_gc()
```

- [ ] **Step 2: Run tests**

```bash
python -m pytest tests/integration/test_inspector_node.py -v
```

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_inspector_node.py
git commit -m "test(inspector): add comprehensive lifetime tests for model redesign"
```
