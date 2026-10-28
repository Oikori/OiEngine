#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    class OI_ENGINE_API UUID final {
    public:
        UUID(const UUID& other);
        UUID(UUID&& other) noexcept;
        ~UUID() = default;

        UUID& operator=(const UUID& other);
        UUID& operator=(UUID&& other) noexcept;
        bool operator==(const UUID& other) const;
        bool operator!=(const UUID& other) const;
        bool operator<(const UUID& other) const;
        bool operator>(const UUID& other) const;
        bool operator<=(const UUID& other) const;
        bool operator>=(const UUID& other) const;
        bool operator!() const;

        std::string ToString(bool split = true) const;

        static UUID FromString(std::string_view str);
        static UUID RandomUUID();

    private:
        UUID(uint64_t most, uint64_t least);

        uint64_t mostSignificantBits;
        uint64_t leastSignificantBits;
    };
}
