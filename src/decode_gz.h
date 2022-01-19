/*
This is the API for decoding a gzip (.gz) file.
*/

#ifndef DECODE_GZ

#include "inttypes.h"
#include "inflate.h"

#define DECODE_GZ_IGNORE_ASSERTS
#ifndef DECODE_GZ_IGNORE_ASSERTS
#include "assert.h"
#endif

#define DECODE_GZ_SILENCE
#ifndef DECODE_GZ_SILENCE
#include "stdio.h"
#endif

typedef struct DecodedData {
    char * data;
    uint32_t data_size;
    uint32_t good;
} DecodedData;

DecodedData * decode_gz(
    uint8_t * compressed_bytes,
    uint32_t compressed_bytes_size);

#endif

