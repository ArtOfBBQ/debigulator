#include "decodedimage.h"

void overwrite_subregion(
    DecodedImage * whole_image,
    const DecodedImage * new_image,
    const uint32_t column_count,
    const uint32_t row_count,
    const uint32_t at_column,
    const uint32_t at_row)
{
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
    DecodedImage ** images_to_concat,
    const uint32_t images_to_concat_size,
    uint32_t * out_sprite_rows,
    uint32_t * out_sprite_columns)
{
    assert(images_to_concat_size > 0);
    assert(images_to_concat != NULL);
    assert(images_to_concat[0] != NULL);
    assert(images_to_concat[0]->width > 0);
    assert(images_to_concat[0]->height > 0);
    
    DecodedImage return_value;
    
    if (images_to_concat_size == 1) {
        *out_sprite_rows = 1;
        *out_sprite_columns = 1;
        return_value = *images_to_concat[0];
        return return_value;
    }
    
    uint32_t base_height =
        images_to_concat[0]->height;
    uint32_t base_width =
        images_to_concat[0]->width;
    
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
            
            if (
                images_to_concat[i]->height != base_height
                || images_to_concat[i]->width != base_width)
            {
                printf(
                    "ERROR: images_to_concat[%u]->height of %u mismatched images_to_concat[0]->height of %u AND/OR images_to_concat[%u]->width of %u mismatched images_to_concat[0]->width of %u\n",
                    i,
                    images_to_concat[i]->height,
                    images_to_concat[0]->height,
                    i,
                    images_to_concat[i]->width,
                    images_to_concat[0]->width);
                assert(0);
            }
            
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
    printf(
        "overwrote subregions, concatenation complete. New dimensions: [%u,%u]\n",
        return_value.width,
        return_value.height);
    
    return return_value;
}

