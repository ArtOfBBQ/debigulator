#include "decodedimage.h"

DecodedImage downsize_image_to_scalar(
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
    #endif
    
    DecodedImage return_value;
    
    return_value.width =
        (uint32_t)((float)original->width * x_scalar);
    return_value.height =
        (uint32_t)((float)original->height * y_scalar);
    #ifndef DECODED_IMAGE_IGNORE_ASSERTS
    assert(return_value.width > 0);
    assert(return_value.height > 0);
    #endif
    return_value.pixel_count =
        return_value.width * return_value.height;
    return_value.rgba_values_size =
        return_value.pixel_count * 4;
    return_value.rgba_values =
        (uint8_t *)malloc(sizeof(return_value.rgba_values_size));
    
    float pixels_per_x = original->width / return_value.width;
    float pixels_per_y = original->height / return_value.height;
    
    // w,h is the width, height position in the new image
    for (uint32_t w = 0; w < return_value.width; w++) {
        for (uint32_t h = 0; h < return_value.height; h++) {
            
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
                    // assert(
                    //     (idx + 3) < original->rgba_values_size);
                    #endif
                    
                    sum_val_r += original->rgba_values[idx];
                    sum_val_g += original->rgba_values[idx + 1];
                    sum_val_b += original->rgba_values[idx + 2];
                    sum_val_a += original->rgba_values[idx + 3];
                }
            }
            
            uint32_t rv_pixel =
                (h * return_value.width)
                + w;
            #ifndef DECODED_IMAGE_IGNORE_ASSERTS
            assert(rv_pixel >= 0);
            assert(rv_pixel < return_value.pixel_count);
            #endif
            uint32_t i = rv_pixel * 4;
            #ifndef DECODED_IMAGE_IGNORE_ASSERTS
            assert(i >= 0);
            assert((i + 3) < return_value.rgba_values_size);
            #endif
           
            return_value.rgba_values[i] =
                (uint8_t)(sum_val_r / pixels_per_pixel);
            return_value.rgba_values[i+1] =
                (uint8_t)(sum_val_g / pixels_per_pixel);
            return_value.rgba_values[i+2] =
                (uint8_t)(sum_val_b / pixels_per_pixel);
            return_value.rgba_values[i+3] =
                (uint8_t)(sum_val_a / pixels_per_pixel);
        }
    }
    
    return return_value;
}

DecodedImage resize_image_to_width(
    DecodedImage * original,
    uint32_t new_width)
{
    float x_scalar = (float)new_width / (float)original->width;
    
    DecodedImage return_value = downsize_image_to_scalar(
        /* original      : */ original,
        /* float x_scalar: */ x_scalar,
        /* float y_scalar: */ x_scalar);
    
    return return_value;
}

void overwrite_subregion(
    DecodedImage * whole_image,
    const DecodedImage * new_image,
    const uint32_t column_count,
    const uint32_t row_count,
    const uint32_t at_column,
    const uint32_t at_row)
{
    printf("overwriting image at [%u,%u]\n", at_column, at_row);
    assert(at_column > 0);
    assert(at_row > 0);
    
    if (at_column > column_count) {
        printf(
            "can't write at [%u,%u], if only %u total columns\n",
            at_column,
            at_row,
            column_count);
        return;
    }
    
    if (at_row > row_count) {
        printf(
            "can't write at [%u,%u], if only %u total rows\n",
            at_column,
            at_row,
            row_count);
        return;
    }
    
    assert(at_row <= row_count);
    uint32_t expected_width = whole_image->width / column_count;
    uint32_t expected_height = whole_image->height / row_count;
    assert(expected_width * column_count == whole_image->width);
    assert(expected_height * row_count == whole_image->height);
    if (
        (new_image->width != expected_width)
        ||
        (new_image->height != expected_height))
    {
        printf(
            "Error - can't overwrite chunk [%u,%u] of dimensions [%u,%u] for image (%u x %u) with new subimage sized [%u,%u], expected size [%u,%u]\n",
            at_column,
            at_row,
            column_count,
            row_count,
            whole_image->width,
            whole_image->height,
            new_image->width,
            new_image->height,
            expected_width,
            expected_height);
        
        return;
    }
    
    uint32_t slice_size =
        whole_image->rgba_values_size
            / column_count
            / row_count;
    uint32_t slice_width =
        whole_image->width / column_count;
    uint32_t slice_height =
        whole_image->height / row_count;
    uint32_t start_x = 1 + ((at_column - 1) * slice_width);
    uint32_t start_y = 1 + ((at_row - 1) * slice_height);
    uint32_t end_y = start_y + slice_height - 1;
    uint32_t end_x = start_x + slice_width - 1;
    assert(end_x <= whole_image->width);
    assert(end_y <= whole_image->height);
    
    uint32_t i = 0;
    for (
        uint32_t cur_y = start_y;
        cur_y <= end_y;
        cur_y++)
    {
        // printf("cur_y: %u\n", cur_y);
        // get the pixel that's at [start_x, cur_y]
        // copcur_y slice_width pixels
        uint32_t pixel_i =
            ((start_x - 1) * 4)
                + ((cur_y - 1) * whole_image->width * 4);
        assert(pixel_i < whole_image->rgba_values_size);
        for (uint32_t _ = 0; _ < (slice_width * 4); _++)
        {
            if (i >= whole_image->rgba_values_size) {
                break;
            }
            whole_image->rgba_values[pixel_i + _] =
                new_image->rgba_values[i];
            i++;
        }
    }
}

DecodedImage concatenate_images(
    const DecodedImage ** images_to_concat,
    const uint32_t images_to_concat_size,
    uint32_t * out_sprite_rows,
    uint32_t * out_sprite_columns)
{
    assert(images_to_concat_size > 0);
    assert(images_to_concat != NULL);
    assert(images_to_concat[0] != NULL);
    
    DecodedImage return_value;
    
    if (images_to_concat_size == 1) {
        *out_sprite_rows = 1;
        *out_sprite_columns = 1;
        return_value = *images_to_concat[0];
        return return_value;
    }
    
    uint32_t base_height = images_to_concat[0]->height;
    uint32_t base_width = images_to_concat[0]->width;
    printf(
        "expecting images with height/width: [%u,%u]\n",
        base_height,
        base_width);
    
    while (1)
    {
       *out_sprite_columns += 1;
       if (*out_sprite_columns * *out_sprite_rows
            >= images_to_concat_size)
       {
            break;
       }
       
       *out_sprite_rows += 1;
       if (*out_sprite_columns * *out_sprite_rows
            >= images_to_concat_size)
       {
            break;
       }
    }
    
    return_value.width =
        (*out_sprite_columns) * base_width;
    return_value.height =
        (*out_sprite_rows) * base_height;
    
    return_value.rgba_values_size =
        base_width * base_height
        * (*out_sprite_rows)
        * (*out_sprite_columns)
        * 4;
    return_value.rgba_values =
        (uint8_t *)malloc(return_value.rgba_values_size); 
    
    printf(
        "created return_value [width,height] = [%u,%u] with rgba_values_size: %u\n",
        return_value.width,
        return_value.height,
        return_value.rgba_values_size);
    
    uint32_t i = 0; 
    for (
        uint32_t at_y = 1;
        at_y <= *out_sprite_rows;
        at_y++)
    {
        for (
            uint32_t at_x = 1;
            at_x <= *out_sprite_columns;
            at_x++)
        {
            if (i >= images_to_concat_size) {
                break;
            }
            
            assert(images_to_concat[i]->height == base_height);
            assert(images_to_concat[i]->width == base_width);
            overwrite_subregion(
                /* whole_image: */ &return_value,
                /* new_image: */ images_to_concat[i],
                /* column_count: */ *out_sprite_columns,
                /* row_count: */ *out_sprite_rows,
                /* at_x: */ at_x,
                /* at_y: */ at_y);
            i++;
        }
    }
    
    return return_value;
}

