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

static DecodedImage * downsize_image_to_scalar(
    DecodedImage * original,
    float x_scalar,
    float y_scalar)
{
    #ifndef DECODED_IMAGE_IGNORE_ASSERTS
    assert(x_scalar < 1);
    assert(y_scalar < 1);
    assert(original != NULL);
    assert(original->rgba_values != NULL);
    assert(original->rgba_values_size > 0);
    assert(original->width > 0);
    assert(original->height > 0);
    assert(
        original->pixel_count == original->rgba_values_size / 4);
    #endif
    
    DecodedImage * return_value = malloc(sizeof(DecodedImage));
    
    return_value->width =
        (uint32_t)((float)original->width * x_scalar);
    return_value->height =
        (uint32_t)((float)original->height * y_scalar);
    #ifndef DECODED_IMAGE_IGNORE_ASSERTS
    assert(return_value->width > 0);
    assert(return_value->height > 0);
    #endif
    return_value->pixel_count =
        return_value->width * return_value->height;
    return_value->rgba_values_size =
        return_value->pixel_count * 4;
    return_value->rgba_values =
        malloc(sizeof(return_value->rgba_values_size));
    
    float pixels_per_x = original->width / return_value->width;
    float pixels_per_y = original->height / return_value->height;
    
    // w,h is the width, height position in the new image
    for (uint32_t w = 0; w < return_value->width; w++) {
        for (uint32_t h = 0; h < return_value->height; h++) {
            
            // first_x, last_x etc. are the positions
            // of the relevant pixels in the original image
            uint32_t first_x =
                (uint32_t)((float)w * pixels_per_x);
            uint32_t last_x =
                (uint32_t)((float)(w + 1) * pixels_per_x);
            uint32_t first_y =
                (uint32_t)((float)h * pixels_per_y);
            uint32_t last_y =
                (uint32_t)((float)(h + 1) * pixels_per_y);
            uint32_t pixels_per_pixel = 1;
            #ifndef DECODED_IMAGE_IGNORE_ASSERTS
            assert(first_x >= 0);
            assert(last_x <= original->width);
            assert(first_y >= 0);
            assert(last_y <= original->height);
            #endif
            
            uint32_t sum_val_r = 0;
            uint32_t sum_val_g = 0;
            uint32_t sum_val_b = 0;
            uint32_t sum_val_a = 0;
           
            for (
                uint32_t orig_x = first_x;
                orig_x <= last_x;
                orig_x++) 
            {
                for (
                    uint32_t orig_y = first_y;
                    orig_y <= last_y;
                    orig_y++) 
                {
                    uint32_t orig_pixel =
                        (orig_y * original->width)
                        + orig_x;
                    if (orig_pixel >= original->pixel_count) {
                        orig_pixel = original->pixel_count - 1;
                    }
                    #ifndef DECODED_IMAGE_IGNORE_ASSERTS
                    assert(orig_pixel >= 0);
                    assert(orig_pixel < original->pixel_count);
                    #endif
                    uint32_t idx = orig_pixel * 4;
                    
                    #ifndef DECODED_IMAGE_IGNORE_ASSERTS
                    assert(idx >= 0);
                    assert(
                        (idx + 3) < original->rgba_values_size);
                    #endif
                    
                    sum_val_r += original->rgba_values[idx];
                    sum_val_g += original->rgba_values[idx + 1];
                    sum_val_b += original->rgba_values[idx + 2];
                    sum_val_a += original->rgba_values[idx + 3];
                }
            }
            
            uint32_t rv_pixel =
                (h * return_value->width)
                + w;
            #ifndef DECODED_IMAGE_IGNORE_ASSERTS
            assert(rv_pixel >= 0);
            assert(rv_pixel < return_value->pixel_count);
            #endif
            uint32_t i = rv_pixel * 4;
            #ifndef DECODED_IMAGE_IGNORE_ASSERTS
            assert(i >= 0);
            assert((i + 3) < return_value->rgba_values_size);
            #endif
           
            return_value->rgba_values[i] =
                (uint8_t)(sum_val_r / pixels_per_pixel);
            return_value->rgba_values[i+1] =
                (uint8_t)(sum_val_g / pixels_per_pixel);
            return_value->rgba_values[i+2] =
                (uint8_t)(sum_val_b / pixels_per_pixel);
            return_value->rgba_values[i+3] =
                (uint8_t)(sum_val_a / pixels_per_pixel);
        }
    }
    
    return return_value;
}

static DecodedImage * resize_image_to_width(
    DecodedImage * original,
    uint32_t new_width)
{
    float x_scalar = (float)new_width / (float)original->width;
    
    DecodedImage * return_value = downsize_image_to_scalar(
        /* original      : */ original,
        /* float x_scalar: */ x_scalar,
        /* float y_scalar: */ x_scalar);
    
    return return_value;
}

#undef bool32_t

#endif

