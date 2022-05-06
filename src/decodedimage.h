/*
This file contains an image structure and some methods
to play with it.
*/

#ifndef DECODED_IMAGE_H
#define DECODED_IMAGE_H

#define bool32_t uint32_t

#include "inttypes.h"
#include "stdlib.h"

#ifndef DECODED_IMAGE_SILENCE
#include "stdio.h"
#endif

#ifndef DECODED_IMAGE_IGNORE_ASSERTS
#include "assert.h"
#endif

typedef struct DecodedImage {
    uint8_t * rgba_values;
    uint32_t rgba_values_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_count; // rgba_values_size / 4
    bool32_t good;
} DecodedImage;

DecodedImage downsize_image_to_scalar(
    DecodedImage * original,
    float x_scalar,
    float y_scalar);

DecodedImage resize_image_to_width(
    DecodedImage * original,
    uint32_t new_width);

/*
you would overwrite the right half of the image by setting:
row_count=1
column_count=2
at_column=2,
at_row=1
*/
void overwrite_subregion(
    DecodedImage * whole_image,
    const DecodedImage * new_image,
    const uint32_t column_count,
    const uint32_t row_count,
    const uint32_t at_column,
    const uint32_t at_row);

DecodedImage concatenate_images(
    const DecodedImage ** images_to_concat,
    const uint32_t images_to_concat_size,
    uint32_t * out_sprite_rows,
    uint32_t * out_sprite_columns);

#endif

