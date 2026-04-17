#pragma once

#include <QDebug>

#include <exception>
#include <string_view>
#include <utility>

// SQP_SAFE_SLOT_BEGIN / SQP_SAFE_SLOT_END — try/catch guards for Qt slot
// lambda bodies. Prevents uncaught C++ exceptions from propagating into the
// Qt event loop (which would call std::terminate).
//
// Usage:
//   connect(sender, &Sender::sig, this, [this](int x) {
//       SQP_SAFE_SLOT_BEGIN
//       do_work(x);
//       SQP_SAFE_SLOT_END("MyClass::on_sig")
//   });

#define SQP_SAFE_SLOT_BEGIN try {
#define SQP_SAFE_SLOT_END(ctx)                                                 \
    }                                                                          \
    catch (const std::exception& _sqp_e)                                       \
    {                                                                          \
        qWarning() << "[safe_slot]" << (ctx) << ":" << _sqp_e.what();          \
    }                                                                          \
    catch (...)                                                                \
    {                                                                          \
        qWarning() << "[safe_slot]" << (ctx) << ": unknown exception";         \
    }

namespace sqp {

template <typename F>
auto safe_slot(F&& f, std::string_view context = "slot")
{
    return [fn = std::forward<F>(f), ctx = std::string(context)]
           (auto&&... args) mutable -> void {
        try
        {
            fn(std::forward<decltype(args)>(args)...);
        }
        catch (const std::exception& e)
        {
            qWarning() << "[safe_slot]" << ctx.c_str() << ":" << e.what();
        }
        catch (...)
        {
            qWarning() << "[safe_slot]" << ctx.c_str() << ": unknown exception";
        }
    };
}

} // namespace sqp
