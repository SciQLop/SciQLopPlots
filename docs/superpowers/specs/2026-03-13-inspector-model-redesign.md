# Inspector Model Redesign

## Problem

The current Inspector MVC subsystem has fundamental ownership and lifetime bugs that cause crashes, use-after-free, and stale UI. The root causes are:

1. **Dual ownership**: `PlotsModelNode` destructor calls `deleteLater()` on the wrapped domain object, but that object is already owned by the Qt widget hierarchy.
2. **Double `destroyed` connection**: Both the node constructor and `InspectorBase::connect_node` connect `obj->destroyed`, causing double-fire on destruction.
3. **Dangling lambda captures**: Raw pointer captures in model lambdas become dangling after node deletion.
4. **Nested `beginInsertRows`**: Recursive `addNode` nests begin/end row signals — invalid QAbstractItemModel API usage.
5. **Diff-based child sync**: `updateNodeChildren` diffs old vs new child lists on every change, with index drift during mutation and fragile name-based matching.
6. **No cleanup on removal**: Signal connections from domain objects to nodes are never disconnected when nodes are removed.
7. **Stale properties panel**: `PropertiesView` holds a delegate widget after the underlying object is destroyed, with no auto-clearing.
8. **Boilerplate-heavy type registration**: One `InspectorBase` subclass + one `PropertyDelegateFactory` subclass per type, each overriding ~6 virtual methods, most trivially.

## Design

### Decisions

- **Location**: Everything stays in SciQLopPlots. Concrete inspectors need intimate knowledge of plot internals.
- **Singleton model**: Kept. One `PlotsModel` per application. Multiple views can share it (standard Qt MVC).
- **Ownership contract**: Nodes *observe* domain objects (via `QPointer`), never own them. Deletion from the tree *requests* object deletion. Object destruction *triggers* node removal. Bidirectional lifetime binding, not ownership.
- **Child sync**: Explicit add/remove signals replace the diff-based `updateNodeChildren`.
- **Type registration**: Data-driven `TypeDescriptor` struct replaces the class-per-type `InspectorBase` hierarchy.
- **Python extension API**: Both `TypeRegistry` and `DelegateRegistry` are callable from Python, so SciQLop can register custom inspectable types and delegate widgets in pure Python.

### Example C++ Registration

```cpp
TypeRegistry::instance().register_type<SciQLopPlot>({
    .children = [](QObject* obj) {
        auto plot = qobject_cast<SciQLopPlot*>(obj);
        QList<QObject*> c = {plot->x_axis(), plot->y_axis()};
        if (plot->has_colormap()) { c << plot->z_axis() << plot->y2_axis(); }
        for (auto p : plot->plottables()) c << p;
        return c;
    },
    .connect_children = [](QObject* obj, auto add, auto remove)
            -> QList<QMetaObject::Connection> {
        auto plot = qobject_cast<SciQLopPlot*>(obj);
        return {
            QObject::connect(plot, &SciQLopPlot::graph_added, plot, add),
            QObject::connect(plot, &SciQLopPlot::graph_removed, plot, remove),
        };
    },
    .set_selected = [](QObject* obj, bool s) {
        qobject_cast<SciQLopPlot*>(obj)->set_selected(s);
    },
    .flags = Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled
});
```

### TypeDescriptor

A single struct per inspectable type, stored in a registry keyed by Qt class name:

```cpp
struct TypeDescriptor {
    // Return initial children when node is first added
    std::function<QList<QObject*>(QObject*)> children;

    // Wire add/remove signals from the domain object to model callbacks.
    // The model provides add_child and remove_child lambdas.
    // MUST return the list of QMetaObject::Connection handles so the model
    // can disconnect them explicitly when the node is removed.
    std::function<QList<QMetaObject::Connection>(
        QObject* obj,
        std::function<void(QObject*)> add_child,
        std::function<void(QObject*)> remove_child)> connect_children;

    // Optional
    std::function<QIcon(const QObject*)> icon = {};
    std::function<QString(const QObject*)> tooltip = {};
    std::function<void(QObject*, bool)> set_selected = {};

    bool deletable = true;
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
};
```

Registrations are assumed to be immortal (live for the application's lifetime). `TypeRegistry` and `DelegateRegistry` are populated at startup before any model operations. Concurrent registration while the model is active is not supported.

For Python registrations: Python callables stored in `std::function` members must be prevented from being garbage-collected. The C++ side wraps them in ref-counted holders (same pattern as `GetDataPyCallable` in `PythonInterface.hpp`).

### TypeRegistry

```cpp
class TypeRegistry {
    QMap<QString, TypeDescriptor> m_descriptors;
public:
    static TypeRegistry& instance();

    // C++ registration (template extracts className from staticMetaObject)
    template<typename T>
    void register_type(TypeDescriptor desc);

    // Python-friendly registration (explicit type name string)
    void register_type(const QString& type_name, TypeDescriptor desc);

    // Lookup walking the metaObject chain (same as current Inspectors::inspector)
    const TypeDescriptor* descriptor(const QObject* obj) const;
};
```

### DelegateRegistry

```cpp
class DelegateRegistry {
    QMap<QString, std::function<QWidget*(QObject*, QWidget*)>> m_factories;
public:
    static DelegateRegistry& instance();

    template<typename DelegateType>
    void register_type();

    // Python-friendly: register a callable that returns QWidget*
    void register_type(const QString& type_name,
                       std::function<QWidget*(QObject*, QWidget*)> factory);

    QWidget* create_delegate(QObject* obj, QWidget* parent = nullptr) const;
};
```

The delegate factory returns `QWidget*` (not `PropertyDelegateBase*`). `PropertyDelegateBase` remains available as a convenience base class but is not required. Python delegates are plain `QWidget` subclasses.

### PlotsModelNode

Simplified, no ownership, no connections in constructor:

```cpp
class PlotsModelNode : public QObject {
    Q_OBJECT
    QPointer<QObject> m_obj;
    const TypeDescriptor* m_descriptor;
    QList<PlotsModelNode*> m_children;
    QList<QMetaObject::Connection> m_connections; // all signal connections for this node

public:
    PlotsModelNode(QObject* obj, const TypeDescriptor* desc, QObject* parent = nullptr);
    ~PlotsModelNode(); // Only deletes child nodes, NEVER touches m_obj

    // Tree structure accessors (same as current: child, children_count, parent_node, etc.)
    // ...

    QObject* object() const;
    const TypeDescriptor* descriptor() const;

    void add_connections(const QList<QMetaObject::Connection>& conns);
    void disconnect_all(); // disconnects all stored connections

    bool deletable() const; // from descriptor
    Qt::ItemFlags flags() const; // from descriptor
    QIcon icon() const; // from descriptor
    QString tooltip() const; // from descriptor
};
```

The node stores all `QMetaObject::Connection` handles created by `addNode` (destroyed, objectNameChanged, connect_children results). `disconnect_all()` severs them all, called by `removeRows` before deletion.

### PlotsModel — addNode

All wiring happens here, not in the node or inspector:

```
addNode(parent_node, obj):
  1. Look up TypeDescriptor for obj
  2. beginInsertRows(parent_index, row, row)
  3. Create PlotsModelNode(obj, descriptor, parent_node)
  4. Append to parent's children list
  5. endInsertRows()
  6. Connect obj->destroyed to guarded lambda:
       [QPointer<PlotsModelNode> node, QPointer<PlotsModelNode> parent, model]
       → if node and parent alive: removeRows(...)
       Store returned QMetaObject::Connection in node->m_connections
  7. Connect obj->objectNameChanged to name update + dataChanged
       Store returned QMetaObject::Connection in node->m_connections
  8. If descriptor->connect_children exists, call it with:
       add_child = [model, QPointer<PlotsModelNode> node](QObject* child) {
           if (node) model->addNode(node, child);
       }
       remove_child = [model, QPointer<PlotsModelNode> node](QObject* child) {
           if (node) model->removeChildByObject(node, child);
       }
       Store ALL returned QMetaObject::Connection handles in node->m_connections
  9. For each child in descriptor->children(obj):
       addNode(this_node, child)   // separate begin/endInsertRows per child
```

Step 9 happens after step 5. Each recursive call is its own begin/end pair — no nesting.

All lambdas in step 8 capture `QPointer<PlotsModelNode>` (not raw pointers) to guard against the node being removed while the domain object is still alive (e.g. non-deletable types or reparenting scenarios).

### PlotsModel — removeRows

The caller must pre-filter non-deletable rows. `removeRows` does not skip rows — all rows in the range are removed. This avoids the begin/endRemoveRows count mismatch bug from the current implementation.

```
removeRows(row, count, parent_index):
  1. Get parent node from index
  2. beginRemoveRows(parent_index, row, row + count - 1)
  3. For each row (reverse order to keep indices stable):
     a. Get child node
     b. node->disconnect_all()  // sever ALL stored QMetaObject::Connections
     c. Capture raw obj pointer: QObject* obj = node->object()
     d. Remove node from parent's children list
     e. Emit node_removed(obj)  // BEFORE deleting node, while obj pointer is still valid
     f. delete node  // destructor cleans up child nodes only, never touches obj
     g. If obj is non-null: obj->deleteLater()  // tree-initiated delete
        (if null, object already dead — external delete path)
  4. endRemoveRows()
```

Step (b) is critical: `disconnect_all()` severs all connections via stored `QMetaObject::Connection` handles. This means `obj->deleteLater()` in step (g) will fire `destroyed` on the next event loop tick, but no handler is connected — no re-entrancy.

Step (e) emits `node_removed` with the raw `obj` pointer captured in step (c), before the QPointer in the node is affected. This allows `PropertiesView` to match against its `m_currentObject`.

Note: between `deleteLater()` (step g) and actual object destruction, the domain object is alive but fully disconnected from the model. Any view repaint during this window sees a consistent model (the node is already gone). This inconsistency window is acceptable.

**Recursive child cleanup**: step (f) `delete node` triggers `~PlotsModelNode`, which recursively deletes child nodes. Before deleting each child, the destructor must call `child->disconnect_all()` to sever connections, and if `child->object()` is non-null, call `child->object()->deleteLater()`. This mirrors the removeRows logic but without begin/endRemoveRows (the parent node is already being removed from the model, so no model notifications are needed for its subtree). The destructor also emits `node_removed(obj)` for each child with a live object so PropertiesView can clear stale delegates.

### PlotsModel — removeChildByObject

Helper used by the `remove_child` lambda in `connect_children`:

```
removeChildByObject(parent_node, obj):
  1. Find row where parent_node->child(row)->object() == obj
  2. If not found: return (object was never added or already removed)
  3. Call removeRows(row, 1, make_index(parent_node))
```

This is a convenience wrapper. The `removeRows` call handles disconnect, deletion, and model notification.

### PropertiesView Cleanup

`PropertiesView::m_delegateWidget` changes type from `PropertyDelegateBase*` to `QWidget*`, since `DelegateRegistry` returns `QWidget*`.

The model emits `node_removed(QObject* obj)` during `removeRows` (step e), with the raw pointer captured before node deletion. `PropertiesView` connects to this signal and clears its delegate widget if the removed object matches the currently displayed one:

```cpp
connect(PlotsModel::instance(), &PlotsModel::node_removed, this,
    [this](QObject* obj) {
        if (m_delegateWidget && m_currentObject == obj) {
            delete m_delegateWidget;
            m_delegateWidget = nullptr;
            m_currentObject = nullptr;
        }
    });
```

### Python Extension API

From SciQLop (pure Python):

```python
from SciQLopPlots import TypeRegistry, DelegateRegistry

# Register a custom type for the inspector tree
# Note: Python connect_children does NOT return connection handles.
# The Python wrapper in __init__.py handles this differently:
# it wraps the user's callable, calls it, and captures the
# QMetaObject::Connection handles internally by connecting through
# a C++ trampoline that returns the handles to the C++ side.
TypeRegistry.instance().register_type(
    type_name="MyDataSource",
    children=lambda obj: obj.findChildren(QObject),
    connect_children=lambda obj, add, remove: (
        obj.child_added.connect(add),
        obj.child_removed.connect(remove),
    ),
    deletable=False,
)

# Register a Python widget as the properties delegate
DelegateRegistry.instance().register_type(
    type_name="MyDataSource",
    factory=lambda obj, parent: MyDataSourceWidget(obj, parent),
)
```

For `DelegateRegistry`, the Python-friendly `register_type(type_name, factory)` overload works directly via Shiboken since the callable returns a `QWidget*` (a bound type). No separate `register_type_py` is needed for delegates.

The C++ side wraps Python callables using the same pattern as `GetDataPyCallable` in `PythonInterface.hpp` (ref-counted `PyObject*` holder to prevent GC).

Shiboken cannot directly expose `TypeDescriptor` with `std::function` fields as keyword arguments. A Python-side convenience wrapper in `SciQLopPlots/__init__.py` will provide the ergonomic API shown above, internally calling a C++ method that accepts individual `PyObject*` callables:

```cpp
// C++ method exposed to Python via Shiboken
void TypeRegistry::register_type_py(
    const QString& type_name,
    PyObject* children_callable,
    PyObject* connect_children_callable,  // may be nullptr
    PyObject* icon_callable,              // may be nullptr
    PyObject* tooltip_callable,           // may be nullptr
    PyObject* set_selected_callable,      // may be nullptr
    bool deletable,
    Qt::ItemFlags flags);
```

The Python wrapper in `__init__.py` maps keyword arguments to this call.

### File Changes

**Deleted** (replaced entirely):
- `include/SciQLopPlots/Inspector/InspectorBase.hpp` + `src/InspectorBase.cpp`
- `include/SciQLopPlots/Inspector/Inspectors.hpp` + `src/Inspectors.cpp`
- All 8 files in `include/SciQLopPlots/Inspector/Inspectors/*.hpp` + their `.cpp` files
- `include/SciQLopPlots/Inspector/PropertyDelegates.hpp`

**Rewritten**:
- `include/SciQLopPlots/Inspector/Model/Node.hpp` + `src/Node.cpp`
- `include/SciQLopPlots/Inspector/Model/Model.hpp` + `src/Model.cpp`

**New files**:
- `include/SciQLopPlots/Inspector/Model/TypeDescriptor.hpp` — struct + `TypeRegistry`
- `include/SciQLopPlots/Inspector/Model/DelegateRegistry.hpp` — delegate factory
- `src/Registrations.cpp` — all built-in type + delegate registrations

**Modified**:
- `include/SciQLopPlots/Inspector/View/PropertiesView.hpp` + its `.cpp` — add cleanup on object removal
- `include/SciQLopPlots/Inspector/View/InspectorView.hpp` + its `.cpp` — selection handling cleanup
- `SciQLopPlots/bindings/bindings.xml` — remove deleted classes, add `TypeRegistry`, `DelegateRegistry`, `TypeDescriptor`

**Kept as-is**:
- All delegate widgets (`PropertiesDelegates/*.hpp`, `Delegates/*.hpp`)
- `PropertyDelegateBase.hpp` + `.cpp`
- `View/TreeView.hpp` + `.cpp`
- `View/PropertiesPanel.hpp` + `.cpp`

### Shiboken Bindings

New entries in `bindings.xml`:
- `TypeRegistry` — exposed as object-type with `register_type(str, ...)` method
- `DelegateRegistry` — exposed as object-type with `register_type(str, callable)` method
- `TypeDescriptor` — may need a Python helper class or keyword-argument constructor rather than direct struct exposure

Removed entries:
- `InspectorBase`
- `Inspectors`
- Individual inspector classes (were never in bindings.xml anyway)
