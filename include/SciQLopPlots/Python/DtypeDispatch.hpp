#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

template <typename F>
auto dispatch_dtype(char format_code, F&& func)
{
    switch (format_code)
    {
        case 'f': return func(std::type_identity<float> {});
        case 'd': return func(std::type_identity<double> {});
        case 'b': return func(std::type_identity<int8_t> {});
        case 'B': return func(std::type_identity<uint8_t> {});
        case 'h': return func(std::type_identity<int16_t> {});
        case 'H': return func(std::type_identity<uint16_t> {});
        case 'i': return func(std::type_identity<int32_t> {});
        case 'I': return func(std::type_identity<uint32_t> {});
        case 'l': return func(std::type_identity<long> {});
        case 'L': return func(std::type_identity<unsigned long> {});
        case 'q': return func(std::type_identity<long long> {});
        case 'Q': return func(std::type_identity<unsigned long long> {});
        default:
        {
            std::string msg = "Unsupported numpy dtype format code: '";
            if (format_code == '\0')
                msg += "\\0";
            else
                msg += format_code;
            msg += "'";
            throw std::invalid_argument(msg);
        }
    }
}

// Copy any numeric buffer into a container of double, converting per its dtype.
// SciQLopPyBuffer::data() is double-only and throws (→ std::terminate) on
// float32/int buffers, which Speasy routinely produces.
template <typename Container, typename Buffer>
Container to_double_vector(const Buffer& buffer)
{
    Container out(buffer.flat_size());
    dispatch_dtype(buffer.format_code(),
                   [&](auto tag)
                   {
                       using V = typename decltype(tag)::type;
                       const auto* src = static_cast<const V*>(buffer.raw_data());
                       std::transform(src, src + buffer.flat_size(), out.begin(),
                                      [](V v) { return static_cast<double>(v); });
                   });
    return out;
}
