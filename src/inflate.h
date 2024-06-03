#ifndef INFLATE_H
#define INFLATE_H

/*
This API offers only 1 function: inflate()

It will decompress a buffer of bytes that was compressed using the DEFLATE or
'zlib' algorithm.

DEFLATE is widely used since the 90's. You could decompress the data chunk
inside a gzip (.gz) file, the IDAT (image data) chunks from a .png image, and
many others.
*/

#include "inttypes.h"
#include "stddef.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef INFLATE_SILENCE
#include "stdio.h"
#endif

#ifndef INFLATE_IGNORE_ASSERTS
#include "assert.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void inflate_init(
    void * (* malloc_funcptr)(size_t __size),
    void * (* arg_memset_func)(void *str, int c, size_t n),
    void * (* arg_memcpy_func)(void * dest, const void * src, size_t n));

void inflate_destroy(
    void (* free_funcptr)(void * to_free));

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
  #define INFLATE_SILENCE, the function will complain about insufficient
  memory with printf() 
- temp_working_memory_size: the capacity in bytes of temp_working_memory
- compressed_input: the data to be uncompressed
- compressed_input_size: the capacity in bytes of compressed_input
- out_good: pass a boolean to this. the value will be ignored and
set to 1 on success, and 0 on failure so you can see if inflate() worked
*/
void inflate(
    uint8_t const * recipient,
    const uint64_t recipient_size,
    uint64_t * final_recipient_size,
    uint8_t const * temp_working_memory,
    const uint64_t temp_working_memory_size,
    uint8_t const * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_good);

#ifdef __cplusplus
}
#endif

#endif // INFLATE_H
