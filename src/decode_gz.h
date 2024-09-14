/*
This is the API for decoding a gzip (.gz) file.
*/

#ifndef DECODE_GZ

// #define DECODE_GZ_IGNORE_ASSERTS
#ifndef DECODE_GZ_IGNORE_ASSERTS
#include "assert.h"
#endif

// #define DECODE_GZ_SILENCE
#ifndef DECODE_GZ_SILENCE
#include "stdio.h"
#endif

#include <stddef.h>


#include "inflate.h"


typedef struct DecodedData {
    char * data;
    uint32_t data_size;
    uint32_t good;
} DecodedData;

void init_decode_gz(
    void * (* malloc_funcptr)(size_t __size),
    void * (* arg_memset_func)
        (void *str, int c, size_t n),
    void * (* arg_memcpy_func)
        (void * dest, const void * src, size_t n));

DecodedData * decode_gz(
    uint8_t * compressed_bytes,
    uint32_t compressed_bytes_size);

#endif

