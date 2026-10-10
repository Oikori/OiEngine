#include "UUID.h"

#include <cstdio>
#include <stdexcept>

#include "Random.h"

namespace OiEngine::Math {
    namespace {
        int hexValue(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }
    }

    UUID::UUID(const UUID& other) = default;
    UUID::UUID(UUID&& other) noexcept = default;

    UUID& UUID::operator=(const UUID& other) = default;
    UUID& UUID::operator=(UUID&& other) noexcept = default;

    UUID::UUID(uint64_t most, uint64_t least): mostSignificantBits(most), leastSignificantBits(least) {}

    bool UUID::operator==(const UUID& other) const
    {
        return mostSignificantBits == other.mostSignificantBits &&
               leastSignificantBits == other.leastSignificantBits;
    }

    bool UUID::operator!=(const UUID& other) const
    {
        return !(*this == other);
    }

    bool UUID::operator<(const UUID& other) const
    {
        if (mostSignificantBits != other.mostSignificantBits)
            return mostSignificantBits < other.mostSignificantBits;
        return leastSignificantBits < other.leastSignificantBits;
    }

    bool UUID::operator>(const UUID& other) const
    {
        return other < *this;
    }

    bool UUID::operator<=(const UUID& other) const
    {
        return !(other < *this);
    }

    bool UUID::operator>=(const UUID& other) const
    {
        return !(*this < other);
    }

    bool UUID::operator!() const
    {
        return mostSignificantBits == 0 && leastSignificantBits == 0;
    }

    std::string UUID::ToString(bool split) const
    {
        std::string result;
        const std::string format = split ? "%08x-%04x-%04x-%04x-%012llx" : "%08x%04x%04x%04x%012llx";
        const size_t count = split ? 37 : 33;
        result.resize(count);
        std::snprintf(result.data(), count, format.c_str(),
                      static_cast<unsigned int>(mostSignificantBits >> 32),
                      static_cast<unsigned int>((mostSignificantBits >> 16) & 0xFFFFULL),
                      static_cast<unsigned int>(mostSignificantBits & 0xFFFFULL),
                      static_cast<unsigned int>(leastSignificantBits >> 48),
                      static_cast<unsigned long long>(leastSignificantBits & 0xFFFFFFFFFFFFULL));
        return result;
    }

    UUID UUID::FromString(std::string_view str)
    {
        uint8_t digits[32];
        size_t count = 0;
        for (char c : str) {
            if (c == '-') continue;
            int value = hexValue(c);
            if (value < 0 || count >= 32)
                throw std::invalid_argument("Invalid UUID string");
            digits[count++] = static_cast<uint8_t>(value);
        }
        if (count != 32)
            throw std::invalid_argument("Invalid UUID string");

        uint64_t most = 0;
        uint64_t least = 0;
        for (int i = 0; i < 16; ++i)
            most = (most << 4) | digits[i];
        for (int i = 16; i < 32; ++i)
            least = (least << 4) | digits[i];
        return {most, least};
    }

    UUID UUID::RandomUUID()
    {
        thread_local Random rng;
        auto most = rng.next<uint64_t>(0, UINT64_MAX);
        auto least = rng.next<uint64_t>(0, UINT64_MAX);
        most = (most & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        least = (least & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
        return {most, least};
    }
}