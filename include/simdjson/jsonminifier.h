#ifndef SIMDJSON_JSONMINIFIER_H
#define SIMDJSON_JSONMINIFIER_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "simdjson/padded_string.h"

namespace simdjson {

// Take input from buf and remove useless whitespace, write it to out; buf and
// out can be the same pointer. Result is null terminated,
// return the string length (minus the null termination).
// The accelerated version of this function only runs on AVX2 hardware.
size_t jsonminify(const uint8_t *buf, size_t len, uint8_t *out);


static inline size_t jsonminify(const char *buf, size_t len, char *out) {
    return jsonminify(reinterpret_cast<const uint8_t *>(buf), len, reinterpret_cast<uint8_t *>(out));
}


static inline size_t jsonminify(const std::string_view & p, char *out) {
    return jsonminify(p.data(), p.size(), out);
}

static inline size_t jsonminify(const padded_string & p, char *out) {
    return jsonminify(p.data(), p.size(), out);
}
}
#endif
