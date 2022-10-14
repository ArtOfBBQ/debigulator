/*
The 2 methods declared in this file are my API for decoding BMP files.
*/

#ifndef DECODE_BMP_H
#define DECODE_BMP_H

#include "stdint.h"

#ifndef DECODE_BMP_SILENCE
#include "stdio.h"
#endif

#ifndef DECODE_BMP_IGNORE_ASSERTS
#include "assert.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void get_BMP_width_height(
    const uint8_t * raw_input,
    const uint64_t raw_input_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good);

void decode_BMP(
    const uint8_t * raw_input,
    const uint64_t raw_input_size,
    uint8_t * out_rgba_values,
    const int64_t out_rgba_values_size,
    uint32_t * out_good);

void encode_BMP(
    const uint8_t * rgba,
    const uint64_t rgba_size,
    const uint32_t width,
    const uint32_t height,
    unsigned char * recipient,
    const int64_t recipient_capacity);

#ifdef __cplusplus
}
#endif

#endif // DECODE_BMP_H
