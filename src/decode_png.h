/*
This API offers only the decode_PNG() function, which reads
& decompresses a PNG stream to the RGBA pixel values inside. 

DecodedImage * decode_PNG(
    uint8_t * compressed_bytes,
    uint32_t compressed_bytes_size)

You pass a stream of bytes to "compressed_bytes" that are
the full contents of the .png file

compressed_size_bytes is the size of the array you're passing
(so the size of the whole file in bytes).

[Q: How do I know if reading the PNG was succesful?]
-> The return value, a DecodedImage, has a field 'good' which will
be 1 if reading the PNG was succesful and 0 if it was a failure.

[Q: Do I have any options?]
-> you can use #define PNG_SILENCE, #define PNG_IGNORE_ASSERTS
in the .c file to avoid including "assert.h" (and you'll have no
debugging assert statements)  and "stdio.h" (and you'll have no
messages in the console).

All other code in the .c file is implementation detail.
*/

#ifndef DECODE_PNG_H
#define DECODE_PNG_H

#include "decodedimage.h"
#include "inflate.h"
#include "inttypes.h"

/*
You can find out the width and height of a PNG by inspecting
only the first 26 bytes of a file.

You can run this before running decode_PNG() to find out
exactly how much memory your preallocated DecodedImage *
struct needs

Sets out_width and out_height to 0 if unsuccesful
*/
void get_PNG_width_height(
    const uint8_t * compressed_bytes,
    const uint32_t compressed_bytes_size,
    uint32_t * out_width,
    uint32_t * out_height);

/*
This function expects you to allocate memory for
a DecodedImage and the rgba_values inside. The function
will 'fill in' the results.

DecodedImage->good will be set to 0 if the decoding was
unsuccesful, and to 1 if the decoding was succesful.
*/
void decode_PNG(
    const uint8_t * compressed_bytes,
    const uint64_t compressed_bytes_size,
    DecodedImage * out_preallocated_png);

#endif

