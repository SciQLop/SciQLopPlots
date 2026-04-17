#pragma once
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
