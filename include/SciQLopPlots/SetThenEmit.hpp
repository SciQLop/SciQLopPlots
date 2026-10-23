#pragma once
#include <QObject>
#include <QPointer>
#include <QSignalBlocker>

namespace SciQLopPlots
{
// NeoQCP setters emit last, but the callers here replot() the item after the setter, so a slot
// deleting the item from that signal would leave them touching a dead item. Run the setter with
// the item's signals blocked, then emit once it is done.
// emit_signal should read back the value the item stored, so the signal agrees with the getter
// even if NeoQCP ever clamps. Returns false when the item is gone: nothing may touch it afterwards.
template <typename Item, typename Set, typename Emit>
[[nodiscard]] bool set_then_emit(Item* item, Set&& set, Emit&& emit_signal)
{
    const QPointer<Item> self(item);
    {
        const QSignalBlocker blocker(item);
        set();
    }
    if (self)
        emit_signal();
    return !self.isNull();
}
} // namespace SciQLopPlots
