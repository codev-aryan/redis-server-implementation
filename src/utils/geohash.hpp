#pragma once
#include <cstdint>

class GeoHash {
public:
    static double encode(double latitude, double longitude);

private:
    static uint64_t interleave64(uint32_t xlo, uint32_t ylo);
};