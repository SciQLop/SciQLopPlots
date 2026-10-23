#pragma once
#include <QObject>
#include <QPointer>

namespace SciQLopPlots
{
// NeoQCP setters keep touching the item after emitting their own signal, so a slot deleting the
// item crashes them. Run the setter with the item's signals blocked, then emit once it is done.
// Returns false when the slot deleted the item: nothing may touch it afterwards.
template <typename Item, typename Set, typename Emit>
[[nodiscard]] bool set_then_emit(Item* item, Set&& set, Emit&& emit_signal)
{
    const QPointer<Item> self(item);
    const bool was_blocked = item->blockSignals(true);
    set();
    item->blockSignals(was_blocked);
    emit_signal();
    return !self.isNull();
}
} // namespace SciQLopPlots
