#include "Random.h"

namespace OiEngine::Math {
    Random::Random(): e(std::random_device{}()) {}

    Random::Random(unsigned int seed): e(seed) {}
}
