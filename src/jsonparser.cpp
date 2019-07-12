#include "simdjson/jsonparser.h"
#ifdef _MSC_VER
#include <windows.h>
#include <sysinfoapi.h>
#else
#include <unistd.h>
#endif
#include "simdjson/simdjson.h"
#include "simdjson/simddetection.h"

namespace simdjson {

instruction_set find_best_supported_implementation() {
  uint32_t available_implementations = detectHostSIMDExtensions();
#if defined (__AVX2__) || defined(__SSE4_2__) || (defined(_MSC_VER) && defined(_M_AMD64))
#ifdef __AVX2__
  if (available_implementations & SIMDExtension_AVX2) {
    return instruction_set::avx2;
  }
#endif
#if defined(__SSE4_2__) || (defined(_MSC_VER) && defined(_M_AMD64))
  if ((available_implementations & SIMDExtension_AVX) || (available_implementations & SIMDExtension_SSE)) {
    return instruction_set::sse4_2;
  }
#endif
#elif defined(__ARM_NEON) || (defined(_MSC_VER) && defined(_M_ARM64))
  if (available_implementations | SIMDExtension_NEON) {
    return instruction_set::neon;
  }
#endif
  return instruction_set::none;
}

// Responsible to select the best json_parse implementation
int json_parse_dispatch(const uint8_t *buf, size_t len, ParsedJson &pj, bool reallocifneeded) {
  // Versions for each implementation
#ifdef __AVX2__
  json_parse_functype* avx_implementation = &json_parse_implementation<instruction_set::avx2>;
#endif
#if defined(__SSE4_2__) || (defined(_MSC_VER) && defined(_M_AMD64))
  json_parse_functype* sse4_2_implementation = &json_parse_implementation<instruction_set::sse4_2>;
#endif
#if  defined(__ARM_NEON) || (defined(_MSC_VER) && defined(_M_ARM64))
  json_parse_functype* neon_implementation = &json_parse_implementation<instruction_set::neon>;
#endif

  instruction_set best_implementation = find_best_supported_implementation();
  
  // Selecting the best implementation
  switch (best_implementation) {
#ifdef __AVX2__
  case instruction_set::avx2 :
    json_parse_ptr = avx_implementation;
    break;
#endif
#if defined(__SSE4_2__) || (defined(_MSC_VER) && defined(_M_AMD64))
  case instruction_set::sse4_2 :
    json_parse_ptr = sse4_2_implementation;
    break;
#endif
#if defined(__ARM_NEON) || (defined(_MSC_VER) && defined(_M_ARM64))
  case instruction_set::neon :
    json_parse_ptr = neon_implementation;
    break;
#endif
  default :
    std::cerr << "No implemented simd instruction set supported" << std::endl;
    return simdjson::UNEXPECTED_ERROR;
  }

  return json_parse_ptr(buf, len, pj, reallocifneeded);
}

json_parse_functype *json_parse_ptr = &json_parse_dispatch;

WARN_UNUSED
ParsedJson build_parsed_json(const uint8_t *buf, size_t len, bool reallocifneeded) {
  ParsedJson pj;
  bool ok = pj.allocateCapacity(len);
  if(ok) {
    json_parse(buf, len, pj, reallocifneeded);
  } else {
    std::cerr << "failure during memory allocation " << std::endl;
  }
  return pj;
}
}