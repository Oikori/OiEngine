#pragma once

#include <random>

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    class OI_ENGINE_API Random final {
    public:
        Random();
        explicit Random(unsigned int seed);

        template<typename T>
        T next(T minVal, T maxVal)
        {
            using type = std::conditional_t<
                std::is_floating_point_v<T>,
                std::uniform_real_distribution<T>,
                std::uniform_int_distribution<T>
            >;
            type u(minVal, maxVal);
            return u(e);
        }

    private:
        std::default_random_engine e;
    };
}
