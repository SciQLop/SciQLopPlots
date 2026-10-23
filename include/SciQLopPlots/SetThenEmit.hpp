#pragma once
#include <QObject>
#include <QPointer>
#include <QSignalBlocker>

namespace SciQLopPlots
{
// NeoQCP setters keep touching the item after emitting their own signal, so a slot deleting the
// item crashes them. Run the setter with the item's signals blocked, then emit once it is done.
// Returns false when the slot deleted the item: nothing may touch it afterwards.
template <typename Item, typename Set, typename Emit>
[[nodiscard]] bool set_then_emit(Item* item, Set&& set, Emit&& emit_signal)
{
    const QPointer<Item> self(item);
    {
        const QSignalBlocker blocker(item);
        set();
    }
    emit_signal();
    return !self.isNull();
}
} // namespace SciQLopPlots
