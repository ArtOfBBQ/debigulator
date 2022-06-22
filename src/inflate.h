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
#include "stdio.h"

#define INFLATE_SILENCE
// #define INFLATE_IGNORE_ASSERTS

#ifndef INFLATE_SILENCE
#include "stdio.h"
#endif

#ifndef INFLATE_IGNORE_ASSERTS
#include "assert.h"
#endif

void inflate(
    uint8_t * recipient,
    const uint64_t recipient_size,
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_good);

#endif
