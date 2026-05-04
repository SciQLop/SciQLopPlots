#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace SciQLopPlots::tracing
{

void enable(const std::string& output_path);
void disable();
void flush();
bool is_enabled() noexcept;
void set_thread_name(const std::string& name);

struct ArgValue
{
    enum class Type
    {
        Int,
        Double,
        Bool,
        String
    };
    Type type;
    int64_t i = 0;
    double d = 0.0;
    bool b = false;
    std::string s;
};

class ScopedZone
{
public:
    explicit ScopedZone(const char* name, const char* category = nullptr) noexcept;
    ~ScopedZone() noexcept;

    ScopedZone(const ScopedZone&) = delete;
    ScopedZone& operator=(const ScopedZone&) = delete;
    ScopedZone(ScopedZone&&) = delete;
    ScopedZone& operator=(ScopedZone&&) = delete;

    void add_arg(const char* key, int64_t value);
    void add_arg(const char* key, double value);
    void add_arg(const char* key, bool value);
    void add_arg(const char* key, const std::string& value);

private:
    int64_t start_ns_;
    const char* name_;
    const char* category_;
    std::vector<std::pair<std::string, ArgValue>> args_;
};

uint64_t async_begin(const char* name, const char* category = nullptr);
void async_end(uint64_t id);
void counter(const char* name, double value, const char* category = nullptr);

// Binding-friendly synchronous-zone API.
// sync_zone_begin/sync_zone_end emit a single complete ("X") event.
// Strings are copied so callers (e.g. Python) need not keep them alive.
// Pass the returned handle to add args and to end.
// Returns 0 if tracing is disabled.
uint64_t sync_zone_begin(const std::string& name, const std::string& category = std::string {});
void sync_zone_add_int(uint64_t handle, const std::string& key, int64_t value);
void sync_zone_add_double(uint64_t handle, const std::string& key, double value);
void sync_zone_add_bool(uint64_t handle, const std::string& key, bool value);
void sync_zone_add_str(uint64_t handle, const std::string& key, const std::string& value);
void sync_zone_end(uint64_t handle);

}  // namespace SciQLopPlots::tracing

#define SCIQLOP_TRACE_CONCAT_(a, b) a##b
#define SCIQLOP_TRACE_CONCAT(a, b) SCIQLOP_TRACE_CONCAT_(a, b)

#define SCIQLOP_TRACE_ZONE(name) \
    ::SciQLopPlots::tracing::ScopedZone SCIQLOP_TRACE_CONCAT(_sciqlop_zone_, __LINE__)(name)

#define SCIQLOP_TRACE_ZONE_CAT(name, cat) \
    ::SciQLopPlots::tracing::ScopedZone SCIQLOP_TRACE_CONCAT(_sciqlop_zone_, __LINE__)(name, cat)
