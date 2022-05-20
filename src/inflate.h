/*
This API offers only the "inflate" function to decompress a
buffer of bytes that was compressed using the DEFLATE or
'zlib' algorithm.

DEFLATE is widely used since the 90's, so you could decompress
the data chunk inside a gzip (.gz) file, an IDAT (image data)
    chunk inside a .png image, and many others*.

*(The 'many others' part is untested)
*/

#ifndef INFLATE_H
#define INFLATE_H

#include "inttypes.h"
#include "stdlib.h"

// TODO: remove this, API should show only inflate()
typedef struct DataStream {
    uint8_t * data;
    
    uint32_t bits_left;
    uint8_t bit_buffer;
    
    uint32_t size_left;
} DataStream;

// TODO: remove this, API should show only inflate()
#define consume_struct(type, from) (type *)consume_chunk(from, sizeof(type))
uint8_t * consume_chunk(
    DataStream * from,
    const size_t size_to_consume);

// TODO: remove this, API should show only inflate()
void discard_bits(
    DataStream * from,
    const unsigned int amount);

uint32_t inflate(
    uint8_t * recipient,
    const uint32_t recipient_size,
    DataStream * data_stream,
    const uint32_t compressed_size_bytes);

#endif

