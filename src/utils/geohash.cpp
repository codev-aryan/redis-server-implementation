#include "geohash.hpp"
#include <algorithm>
#include <cmath>

// Redis Geohash Limits
const double GEO_LAT_MIN = -85.05112878;
const double GEO_LAT_MAX = 85.05112878;
const double GEO_LONG_MIN = -180.0;
const double GEO_LONG_MAX = 180.0;

// Spread bits: xxxxxxxx -> x0x0x0x0x0x0x0x0
uint64_t GeoHash::interleave64(uint32_t xlo, uint32_t ylo) {
    static const uint64_t B[] = {0x5555555555555555ULL, 0x3333333333333333ULL,
                                 0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL,
                                 0x0000FFFF0000FFFFULL};
    static const unsigned int S[] = {1, 2, 4, 8, 16};

    uint64_t x = xlo;
    uint64_t y = ylo;

    // Spread x (Lat) -> even bits
    x = (x | (x << S[4])) & B[4];
    x = (x | (x << S[3])) & B[3];
    x = (x | (x << S[2])) & B[2];
    x = (x | (x << S[1])) & B[1];
    x = (x | (x << S[0])) & B[0];

    // Spread y (Long) -> even bits (will be shifted to odd)
    y = (y | (y << S[4])) & B[4];
    y = (y | (y << S[3])) & B[3];
    y = (y | (y << S[2])) & B[2];
    y = (y | (y << S[1])) & B[1];
    y = (y | (y << S[0])) & B[0];

    return x | (y << 1);
}

// Compact bits: x0x0x0x0 -> xxxxxxxx
uint64_t GeoHash::deinterleave64(uint64_t interleaved) {
    uint64_t x = interleaved;

    // 1. Keep only even bits (0, 2, 4...)
    x &= 0x5555555555555555ULL; 

    // 2. Compact 1-bit gaps
    // (x | (x >> 1)) combines neighbors. Mask 0x3333... keeps the valid packed pairs.
    x = (x | (x >> 1)) & 0x3333333333333333ULL; 
    
    // 3. Compact 2-bit gaps
    x = (x | (x >> 2)) & 0x0F0F0F0F0F0F0F0FULL;

    // 4. Compact 4-bit gaps
    x = (x | (x >> 4)) & 0x00FF00FF00FF00FFULL;

    // 5. Compact 8-bit gaps
    x = (x | (x >> 8)) & 0x0000FFFF0000FFFFULL;

    // 6. Compact 16-bit gaps
    x = (x | (x >> 16)) & 0x00000000FFFFFFFFULL;

    return x;
}

double GeoHash::encode(double latitude, double longitude) {
    if (latitude < GEO_LAT_MIN) latitude = GEO_LAT_MIN;
    if (latitude > GEO_LAT_MAX) latitude = GEO_LAT_MAX;
    if (longitude < GEO_LONG_MIN) longitude = GEO_LONG_MIN;
    if (longitude > GEO_LONG_MAX) longitude = GEO_LONG_MAX;

    double lat_offset = (latitude - GEO_LAT_MIN) / (GEO_LAT_MAX - GEO_LAT_MIN);
    double long_offset = (longitude - GEO_LONG_MIN) / (GEO_LONG_MAX - GEO_LONG_MIN);

    lat_offset *= (1ULL << 26);
    long_offset *= (1ULL << 26);

    uint32_t lat_bits = static_cast<uint32_t>(lat_offset);
    uint32_t long_bits = static_cast<uint32_t>(long_offset);

    uint64_t hash = interleave64(lat_bits, long_bits);

    return static_cast<double>(hash);
}

std::pair<double, double> GeoHash::decode(double score) {
    uint64_t hash = static_cast<uint64_t>(score);

    // Lat is at even bits (0, 2, 4...) -> deinterleave directly
    uint64_t lat_bits = deinterleave64(hash);
    
    // Long is at odd bits (1, 3, 5...) -> shift right 1 then deinterleave
    uint64_t long_bits = deinterleave64(hash >> 1);

    double lat_step = (GEO_LAT_MAX - GEO_LAT_MIN) / (1ULL << 26);
    double long_step = (GEO_LONG_MAX - GEO_LONG_MIN) / (1ULL << 26);

    // Coordinate = Min + (Value * Step) + (Step / 2)
    double latitude = GEO_LAT_MIN + (lat_bits * lat_step) + (lat_step / 2);
    double longitude = GEO_LONG_MIN + (long_bits * long_step) + (long_step / 2);

    return {latitude, longitude};
}