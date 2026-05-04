#include "SciQLopPlots/Tracing.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#  include <sys/syscall.h>
#  include <unistd.h>
#endif

namespace SciQLopPlots::tracing
{
namespace
{

    int64_t now_ns() noexcept
    {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }

    int64_t process_id() noexcept
    {
#ifdef __linux__
        return ::getpid();
#else
        return 0;
#endif
    }

    int64_t thread_id() noexcept
    {
#ifdef __linux__
        return ::syscall(SYS_gettid);
#else
        return static_cast<int64_t>(
            std::hash<std::thread::id> {}(std::this_thread::get_id()));
#endif
    }

    std::string escape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            switch (c)
            {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                            static_cast<unsigned>(static_cast<unsigned char>(c)));
                        out += buf;
                    }
                    else
                        out += c;
            }
        }
        return out;
    }

    std::string format_double(double v)
    {
        std::ostringstream s;
        s.precision(17);
        s << v;
        return s.str();
    }

    enum class Phase : uint8_t
    {
        X,
        AsyncBegin,
        AsyncEnd,
        Counter
    };

    struct Event
    {
        Phase phase;
        std::string name;
        std::string category;
        int64_t ts_ns = 0;
        int64_t dur_ns = 0;
        int64_t tid = 0;
        uint64_t async_id = 0;
        double counter_value = 0.0;
        std::vector<std::pair<std::string, ArgValue>> args;
    };

    struct ThreadBuffer
    {
        std::mutex mu;
        std::vector<Event> events;
        int64_t tid = 0;
        std::string name;
    };

    class Tracer
    {
    public:
        static Tracer& instance()
        {
            static Tracer t;
            return t;
        }

        std::atomic<bool> enabled { false };

        void enable(const std::string& path)
        {
            std::lock_guard<std::mutex> lk(state_mu_);
            if (enabled.load(std::memory_order_acquire))
                disable_locked();
            out_.open(path, std::ios::binary | std::ios::trunc);
            if (!out_.is_open())
                return;
            out_ << "{\"displayTimeUnit\":\"ns\",\"traceEvents\":[\n";
            first_event_ = true;
            origin_ns_ = now_ns();
            clear_all_buffers_locked();
            known_tids_.clear();
            emit_process_metadata_locked();
            stop_flush_.store(false);
            enabled.store(true, std::memory_order_release);
            flush_thread_ = std::thread([this] { flush_loop(); });
        }

        void disable()
        {
            std::lock_guard<std::mutex> lk(state_mu_);
            disable_locked();
        }

        void flush()
        {
            if (!enabled.load(std::memory_order_acquire))
                return;
            std::lock_guard<std::mutex> flk(flush_mu_);
            if (!enabled.load(std::memory_order_acquire))
                return;
            drain_all_locked();
            out_.flush();
        }

        void register_buffer(ThreadBuffer* b)
        {
            std::lock_guard<std::mutex> lk(buffers_mu_);
            buffers_.push_back(b);
        }

        int64_t origin_ns() const noexcept { return origin_ns_; }

    private:
        std::mutex state_mu_;   // serializes enable/disable transitions
        std::mutex flush_mu_;   // protects out_, first_event_, origin_ns_, known_tids_ during drains
        std::mutex buffers_mu_; // protects buffers_ vector
        std::vector<ThreadBuffer*> buffers_;
        std::ofstream out_;
        bool first_event_ = true;
        int64_t origin_ns_ = 0;
        std::set<int64_t> known_tids_;
        std::thread flush_thread_;
        std::condition_variable cv_;
        std::mutex cv_mu_;
        std::atomic<bool> stop_flush_ { false };

        void disable_locked()
        {
            if (!enabled.exchange(false, std::memory_order_acq_rel))
                return;
            stop_flush_.store(true, std::memory_order_release);
            cv_.notify_all();
            if (flush_thread_.joinable())
                flush_thread_.join();
            std::lock_guard<std::mutex> flk(flush_mu_);
            drain_all_locked();
            out_ << "\n]}\n";
            out_.flush();
            out_.close();
            first_event_ = true;
        }

        void flush_loop()
        {
            while (!stop_flush_.load(std::memory_order_acquire))
            {
                {
                    std::unique_lock<std::mutex> lk(cv_mu_);
                    cv_.wait_for(lk, std::chrono::milliseconds(250),
                        [&] { return stop_flush_.load(std::memory_order_acquire); });
                }
                if (!enabled.load(std::memory_order_acquire))
                    continue;
                std::lock_guard<std::mutex> flk(flush_mu_);
                if (enabled.load(std::memory_order_acquire))
                    drain_all_locked();
            }
        }

        void clear_all_buffers_locked()
        {
            std::lock_guard<std::mutex> lk(buffers_mu_);
            for (auto* b : buffers_)
            {
                std::lock_guard<std::mutex> lk2(b->mu);
                b->events.clear();
            }
        }

        void drain_all_locked()
        {
            std::vector<ThreadBuffer*> bufs;
            {
                std::lock_guard<std::mutex> lk(buffers_mu_);
                bufs = buffers_;
            }
            for (auto* b : bufs)
            {
                std::vector<Event> local;
                std::string thread_name;
                int64_t tid = b->tid;
                {
                    std::lock_guard<std::mutex> lk(b->mu);
                    local.swap(b->events);
                    thread_name = b->name;
                }
                if (!local.empty() && known_tids_.insert(tid).second)
                    emit_thread_metadata_locked(tid, thread_name);
                for (auto& e : local)
                    emit_event_locked(e);
            }
        }

        void emit_process_metadata_locked()
        {
            std::ostringstream s;
            s << "{\"ph\":\"M\",\"name\":\"process_name\",\"pid\":" << process_id()
              << ",\"tid\":0,\"args\":{\"name\":\"sciqlop\"}}";
            write_raw_locked(s.str());
        }

        void emit_thread_metadata_locked(int64_t tid, const std::string& name)
        {
            std::ostringstream s;
            std::string label = name.empty() ? std::to_string(tid) : name;
            s << "{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":" << process_id()
              << ",\"tid\":" << tid << ",\"args\":{\"name\":\"" << escape(label) << "\"}}";
            write_raw_locked(s.str());
        }

        void emit_event_locked(const Event& e)
        {
            std::ostringstream s;
            const int64_t ts_us = (e.ts_ns - origin_ns_) / 1000;
            switch (e.phase)
            {
                case Phase::X:
                    s << "{\"ph\":\"X\",\"name\":\"" << escape(e.name) << "\"";
                    if (!e.category.empty())
                        s << ",\"cat\":\"" << escape(e.category) << "\"";
                    s << ",\"ts\":" << ts_us << ",\"dur\":" << (e.dur_ns / 1000)
                      << ",\"pid\":" << process_id() << ",\"tid\":" << e.tid;
                    if (!e.args.empty())
                        s << ",\"args\":" << format_args(e.args);
                    s << "}";
                    break;
                case Phase::AsyncBegin:
                    s << "{\"ph\":\"b\",\"name\":\"" << escape(e.name) << "\"";
                    if (!e.category.empty())
                        s << ",\"cat\":\"" << escape(e.category) << "\"";
                    s << ",\"ts\":" << ts_us << ",\"id\":\"0x" << std::hex << e.async_id
                      << std::dec << "\",\"pid\":" << process_id() << ",\"tid\":" << e.tid;
                    if (!e.args.empty())
                        s << ",\"args\":" << format_args(e.args);
                    s << "}";
                    break;
                case Phase::AsyncEnd:
                    s << "{\"ph\":\"e\",\"name\":\"" << escape(e.name) << "\"";
                    if (!e.category.empty())
                        s << ",\"cat\":\"" << escape(e.category) << "\"";
                    s << ",\"ts\":" << ts_us << ",\"id\":\"0x" << std::hex << e.async_id
                      << std::dec << "\",\"pid\":" << process_id() << ",\"tid\":" << e.tid << "}";
                    break;
                case Phase::Counter:
                    s << "{\"ph\":\"C\",\"name\":\"" << escape(e.name) << "\"";
                    if (!e.category.empty())
                        s << ",\"cat\":\"" << escape(e.category) << "\"";
                    s << ",\"ts\":" << ts_us << ",\"pid\":" << process_id()
                      << ",\"tid\":" << e.tid << ",\"args\":{\"" << escape(e.name)
                      << "\":" << format_double(e.counter_value) << "}}";
                    break;
            }
            write_raw_locked(s.str());
        }

        void write_raw_locked(const std::string& json)
        {
            if (!first_event_)
                out_ << ",\n";
            out_ << "  " << json;
            first_event_ = false;
        }

        static std::string format_args(
            const std::vector<std::pair<std::string, ArgValue>>& args)
        {
            std::ostringstream s;
            s << "{";
            bool first = true;
            for (const auto& [k, v] : args)
            {
                if (!first)
                    s << ",";
                first = false;
                s << "\"" << escape(k) << "\":";
                switch (v.type)
                {
                    case ArgValue::Type::Int: s << v.i; break;
                    case ArgValue::Type::Double: s << format_double(v.d); break;
                    case ArgValue::Type::Bool: s << (v.b ? "true" : "false"); break;
                    case ArgValue::Type::String:
                        s << "\"" << escape(v.s) << "\"";
                        break;
                }
            }
            s << "}";
            return s.str();
        }
    };

    struct ThreadLocalBuffer
    {
        ThreadBuffer* buf;
        ThreadLocalBuffer() : buf(new ThreadBuffer())
        {
            buf->tid = thread_id();
            Tracer::instance().register_buffer(buf);
        }
        ~ThreadLocalBuffer() = default;  // intentionally leak: tracer owns lifetime
    };

    ThreadBuffer* tls_buffer()
    {
        thread_local ThreadLocalBuffer tl;
        return tl.buf;
    }

    void push_event(Event e)
    {
        ThreadBuffer* b = tls_buffer();
        e.tid = b->tid;
        std::lock_guard<std::mutex> lk(b->mu);
        b->events.push_back(std::move(e));
    }

    struct EnvAutoEnable
    {
        EnvAutoEnable()
        {
            std::atexit([] {
                if (Tracer::instance().enabled.load(std::memory_order_relaxed))
                    Tracer::instance().disable();
            });
            if (const char* p = std::getenv("SCIQLOP_TRACE"))
                if (p[0] != '\0')
                    Tracer::instance().enable(p);
        }
    };
    EnvAutoEnable s_env_auto_enable;

}  // namespace

void enable(const std::string& path) { Tracer::instance().enable(path); }
void disable() { Tracer::instance().disable(); }
void flush() { Tracer::instance().flush(); }
bool is_enabled() noexcept
{
    return Tracer::instance().enabled.load(std::memory_order_relaxed);
}

void set_thread_name(const std::string& name)
{
    ThreadBuffer* b = tls_buffer();
    std::lock_guard<std::mutex> lk(b->mu);
    b->name = name;
}

ScopedZone::ScopedZone(const char* name, const char* category) noexcept
    : start_ns_(0), name_(name), category_(category)
{
    if (Tracer::instance().enabled.load(std::memory_order_relaxed))
        start_ns_ = now_ns();
}

ScopedZone::~ScopedZone() noexcept
{
    if (start_ns_ == 0)
        return;
    int64_t end = now_ns();
    Event e;
    e.phase = Phase::X;
    e.name = name_ ? name_ : "";
    e.category = category_ ? category_ : "";
    e.ts_ns = start_ns_;
    e.dur_ns = end - start_ns_;
    e.args = std::move(args_);
    push_event(std::move(e));
}

void ScopedZone::add_arg(const char* k, int64_t v)
{
    if (start_ns_ == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::Int;
    a.i = v;
    args_.emplace_back(k ? k : "", std::move(a));
}
void ScopedZone::add_arg(const char* k, double v)
{
    if (start_ns_ == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::Double;
    a.d = v;
    args_.emplace_back(k ? k : "", std::move(a));
}
void ScopedZone::add_arg(const char* k, bool v)
{
    if (start_ns_ == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::Bool;
    a.b = v;
    args_.emplace_back(k ? k : "", std::move(a));
}
void ScopedZone::add_arg(const char* k, const std::string& v)
{
    if (start_ns_ == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::String;
    a.s = v;
    args_.emplace_back(k ? k : "", std::move(a));
}

uint64_t async_begin(const char* name, const char* category)
{
    if (!is_enabled())
        return 0;
    static std::atomic<uint64_t> next_id { 1 };
    uint64_t id = next_id.fetch_add(1, std::memory_order_relaxed);
    Event e;
    e.phase = Phase::AsyncBegin;
    e.name = name ? name : "";
    e.category = category ? category : "";
    e.ts_ns = now_ns();
    e.async_id = id;
    push_event(std::move(e));
    return id;
}

void async_end(uint64_t id)
{
    if (!is_enabled() || id == 0)
        return;
    Event e;
    e.phase = Phase::AsyncEnd;
    e.name = "";
    e.ts_ns = now_ns();
    e.async_id = id;
    push_event(std::move(e));
}

void counter(const char* name, double value, const char* category)
{
    if (!is_enabled())
        return;
    Event e;
    e.phase = Phase::Counter;
    e.name = name ? name : "";
    e.category = category ? category : "";
    e.counter_value = value;
    e.ts_ns = now_ns();
    push_event(std::move(e));
}

namespace
{
    struct OwningZone
    {
        std::string name;
        std::string category;
        int64_t start_ns;
        std::vector<std::pair<std::string, ArgValue>> args;
    };
}  // namespace

uint64_t sync_zone_begin(const std::string& name, const std::string& category)
{
    if (!is_enabled())
        return 0;
    auto* z = new OwningZone { name, category, now_ns(), {} };
    return reinterpret_cast<uint64_t>(z);
}

static OwningZone* as_owning(uint64_t handle) noexcept
{
    return reinterpret_cast<OwningZone*>(handle);
}

void sync_zone_add_int(uint64_t handle, const std::string& key, int64_t value)
{
    if (handle == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::Int;
    a.i = value;
    as_owning(handle)->args.emplace_back(key, std::move(a));
}

void sync_zone_add_double(uint64_t handle, const std::string& key, double value)
{
    if (handle == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::Double;
    a.d = value;
    as_owning(handle)->args.emplace_back(key, std::move(a));
}

void sync_zone_add_bool(uint64_t handle, const std::string& key, bool value)
{
    if (handle == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::Bool;
    a.b = value;
    as_owning(handle)->args.emplace_back(key, std::move(a));
}

void sync_zone_add_str(uint64_t handle, const std::string& key, const std::string& value)
{
    if (handle == 0)
        return;
    ArgValue a;
    a.type = ArgValue::Type::String;
    a.s = value;
    as_owning(handle)->args.emplace_back(key, std::move(a));
}

void sync_zone_end(uint64_t handle)
{
    if (handle == 0)
        return;
    std::unique_ptr<OwningZone> z(as_owning(handle));
    if (!is_enabled())
        return;
    Event e;
    e.phase = Phase::X;
    e.name = std::move(z->name);
    e.category = std::move(z->category);
    e.ts_ns = z->start_ns;
    e.dur_ns = now_ns() - z->start_ns;
    e.args = std::move(z->args);
    push_event(std::move(e));
}

}  // namespace SciQLopPlots::tracing
