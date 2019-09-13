#ifndef ADPCMFORMAT_HPP
#define ADPCMFORMAT_HPP
/*
================= DO NOT EDIT THIS FILE ====================
This file has  been automatically generated by genstructs.py

Please  edit  structs.xml instead, and  re-run genstructs.py
============================================================
*/
#include "StructLoader.hpp"
#include "AdpcmCoeffs.hpp"
struct AdpcmFormat {
  std::uint16_t samplesPerBlock;
  std::uint16_t numCoeffs;
  std::vector<AdpcmCoeffs> coeffs;
};

template <> struct StructLoader<AdpcmFormat> {
  static const char* readBuffer(const char *begin, const char *end, AdpcmFormat &output) {
    if (begin > end) {
      throw DLSynth::Error("Wrong data size", DLSynth::ErrorCode::INVALID_FILE);
    }
    const char *cur_pos = begin;
    cur_pos = StructLoader<std::uint16_t>::readBuffer(cur_pos, end, output.samplesPerBlock);
    cur_pos = StructLoader<std::uint16_t>::readBuffer(cur_pos, end, output.numCoeffs);
    output.coeffs.resize(output.numCoeffs);
    cur_pos = readArray<AdpcmCoeffs>(cur_pos, end, output.numCoeffs, output.coeffs);
    return cur_pos;
  }
};
#endif
