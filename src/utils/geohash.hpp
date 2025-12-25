#pragma once
#include <cstdint>
#include <utility>

class GeoHash {
public:
    static double encode(double latitude, double longitude);
    static std::pair<double, double> decode(double score);

private:
    static uint64_t interleave64(uint32_t xlo, uint32_t ylo);
    static uint64_t deinterleave64(uint64_t interleaved);
};