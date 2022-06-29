#define INFLATE_SILENCE
// #define INFLATE_IGNORE_ASSERTS

/*
This API offers only the "inflate" function to decompress a buffer of bytes 
that was compressed using the DEFLATE or 'zlib' algorithm.

DEFLATE is widely used since the 90's, so you could decompress the data chunk
inside a gzip (.gz) file, the IDAT (image data) chunks from a .png image, and
according to legend many others.
*/

#ifndef INFLATE_H
#define INFLATE_H

#include "inttypes.h"

#ifndef NULL
#define remove_NULL_def
#define NULL nullptr
#endif 

#ifndef INFLATE_SILENCE
#include "stdio.h"
#endif

#ifndef INFLATE_IGNORE_ASSERTS
#include "assert.h"
#endif

/*
This function decompresses data was compressed using the DEFLATE algorithm.

- recipient: the receiving memory to uncompress to
- recipient_size: the capacity in bytes of recipient
- final_recipient_size: will be filled in with the actual size of the recipient
after decompressing everything.
- temp_working_memory: will be used to store some hashmaps
that are only useful while the functions runs. You can overwrite
, free, or pass somewhere else immediately after. The function will fail when
  the working memory is insufficient. If you comment out
  #define INFLATE_SILENCE, the function will complain about insufficient memory
  with printf() 
- temp_working_memory_size: the capacity in bytes of temp_working_memory
- compressed_input: the data to be uncompressed
- compressed_input_size: the capacity in bytes of compressed_input
- out_good: pass a boolean to this. the value will be ignored and
set to 1 on success, and 0 on failure so you can see if inflate() worked
*/
void inflate(
    const uint8_t * recipient,
    const uint64_t recipient_size,
    uint64_t * final_recipient_size,
    const uint8_t * temp_working_memory,
    const uint64_t temp_working_memory_size,
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_good);

#endif
