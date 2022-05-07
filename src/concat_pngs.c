#include "decode_png.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_write.h"

DecodedImage * read_png_from_disk(const char * filename) {
    printf("read from disk: %s\n", filename);
    FILE * imgfile = fopen(
        filename,
        "rb");
    
    fseek(imgfile, 0, SEEK_END);
    unsigned long fsize = (unsigned long)ftell(imgfile);
    fseek(imgfile, 0, SEEK_SET);
    
    uint8_t * buffer = (uint8_t *)malloc(fsize);
    uint8_t * start_of_buffer = buffer;
    
    size_t bytes_read = fread(
        /* ptr: */
            buffer,
        /* size of each element to be read: */
            1,
        /* nmemb (no of members) to read: */
            fsize,
        /* stream: */
            imgfile);
    
    #ifndef HELLOPNG_SILENCE
    printf(
        "bytes read from raw file: %zu\n",
        bytes_read);
    #endif
    
    fclose(imgfile);
    if (bytes_read != fsize) {
        #ifndef HELLOPNG_SILENCE
        printf("Error - expected bytes read equal to fsize\n");
        #endif
        return 1;
    }
    
    uint8_t * buffer_copy = start_of_buffer;
    
    DecodedImage * return_value = decode_PNG(
        /* compressed_bytes: */
            buffer_copy,
        /* compressed_bytes_size: */
            (uint32_t)bytes_read);
    
    free(buffer);
    
    return return_value;
}

int main() {
    printf("concat_pngs main()\n");

    DecodedImage * to_concat[3];
    uint32_t to_concat_size = 3;
    
    to_concat[0] = read_png_from_disk("structuredart1.png");
    printf("fs_angrymob height: %u\n", to_concat[0]->height);
    to_concat[1] = read_png_from_disk("structuredart2.png");
    to_concat[2] = read_png_from_disk("structuredart3.png");
   
    uint32_t sprite_rows;
    uint32_t sprite_columns; 
    DecodedImage recipient = concatenate_images(
        /* to_concat: */ &to_concat,
        /* to_concat_size: */ to_concat_size,
        /* out_sprite_rows: */ &sprite_rows,
        /* out_sprite_columns: */ &sprite_columns);
    
    assert(recipient.width > 0);
    assert(recipient.height > 0);
    assert(recipient.rgba_values_size > 0);
    printf("got concatenated result with width: %u, height: %u, rgba_values_size: %u\n",
        recipient.width,
        recipient.height,
        recipient.rgba_values_size);
    
    int result = stbi_write_png(
        /* char const * filename : */
            "concatenated_output.png",
        /* int w : */
            recipient.width,
        /* int h : */
            recipient.height,
        /* int comp : */
            4,
        /* const void *data : */
            recipient.rgba_values,
        /* int stride_in_bytes : */
            recipient.rgba_values_size /
                    recipient.height);
}

