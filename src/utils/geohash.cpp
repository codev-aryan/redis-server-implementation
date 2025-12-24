#include "geohash.hpp"
#include <algorithm>

// Redis Geohash Limits
// Latitude: +/- 85.05112878 (Web Mercator limits)
// Longitude: +/- 180.0
const double GEO_LAT_MIN = -85.05112878;
const double GEO_LAT_MAX = 85.05112878;
const double GEO_LONG_MIN = -180.0;
const double GEO_LONG_MAX = 180.0;

// Interleave lower 32 bits of x and y into a 64-bit integer
// x occupies even bits (0, 2, 4...), y occupies odd bits (1, 3, 5...)
uint64_t GeoHash::interleave64(uint32_t xlo, uint32_t ylo) {
    static const uint64_t B[] = {0x5555555555555555ULL, 0x3333333333333333ULL,
                                 0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL,
                                 0x0000FFFF0000FFFFULL};
    static const unsigned int S[] = {1, 2, 4, 8, 16};

    uint64_t x = xlo;
    uint64_t y = ylo;

    // Spread bits with 0s in between (Morton Code / Z-Order Curve)
    x = (x | (x << S[4])) & B[4];
    y = (y | (y << S[4])) & B[4];

    x = (x | (x << S[3])) & B[3];
    y = (y | (y << S[3])) & B[3];

    x = (x | (x << S[2])) & B[2];
    y = (y | (y << S[2])) & B[2];

    x = (x | (x << S[1])) & B[1];
    y = (y | (y << S[1])) & B[1];

    x = (x | (x << S[0])) & B[0];
    y = (y | (y << S[0])) & B[0];

    // Interleave: x=lat (even), y=long (odd)
    // Note: Standard Redis implementation puts lat at even bits, long at odd bits
    return x | (y << 1);
}

double GeoHash::encode(double latitude, double longitude) {
    // 1. Clamp coordinates to range
    if (latitude < GEO_LAT_MIN) latitude = GEO_LAT_MIN;
    if (latitude > GEO_LAT_MAX) latitude = GEO_LAT_MAX;
    if (longitude < GEO_LONG_MIN) longitude = GEO_LONG_MIN;
    if (longitude > GEO_LONG_MAX) longitude = GEO_LONG_MAX;

    // 2. Normalize to 0..1 range
    double lat_offset = (latitude - GEO_LAT_MIN) / (GEO_LAT_MAX - GEO_LAT_MIN);
    double long_offset = (longitude - GEO_LONG_MIN) / (GEO_LONG_MAX - GEO_LONG_MIN);

    // 3. Scale to 26 bits (2^26 = 67108864)
    // Redis uses 52 bits total (26 lat + 26 long)
    lat_offset *= (1ULL << 26);
    long_offset *= (1ULL << 26);

    uint32_t lat_bits = static_cast<uint32_t>(lat_offset);
    uint32_t long_bits = static_cast<uint32_t>(long_offset);

    // 4. Interleave bits
    uint64_t hash = interleave64(lat_bits, long_bits);

    // 5. Return as double (Sorted Sets use double scores)
    // A 52-bit integer fits exactly into a double's mantissa (which holds 53 bits)
    return static_cast<double>(hash);
}