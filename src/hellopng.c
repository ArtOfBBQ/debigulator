/*
This file is meant as an example of how to use the "png.h"
header to read pixels from a .png file.
*/

#include "decode_png.h"
#include "decodedimage.h"
#include "stdio.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_write.h"

// #define HELLOPNG_SILENCE

DecodedImage read_png_from_disk(const char * filename) {
    DecodedImage return_value;
    return_value.good = 0;
    
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
        return return_value;
    }
    
    uint8_t * buffer_copy = start_of_buffer;

    uint32_t png_width;
    uint32_t png_height;
    get_PNG_width_height(
        /* uint8_t * compressed_bytes: */
            buffer_copy,
        /* uint32_t compressed_bytes_size: */
            bytes_read,
        /* uint32_t * width_out: */
            &png_width,
        /* uint32_t * height_out: */
            &png_height);
    
    assert(png_width > 0);
    assert(png_height > 0);
    
    return_value.rgba_values_size =
        png_width * png_height * 4;
    printf(
        "allocating %u bytes for rgba_values because [w/h] was: [%u,%u]\n",
        return_value.rgba_values_size,
        png_width,
        png_height);
    return_value.rgba_values =
        (uint8_t *)malloc(return_value.rgba_values_size);
    
    assert(buffer_copy == start_of_buffer);
    decode_PNG(
        /* compressed_bytes: */
            buffer_copy,
        /* compressed_bytes_size: */
            bytes_read,
        /* DecodedImage * out_preallocated_png: */
            &return_value);
    
    return return_value;
}

int main(int argc, const char * argv[]) 
{
    if (argc != 2) {
        #ifndef HELLOPNG_SILENCE
        printf("Please supply 1 argument (png file name)\n");
        printf("Got:");
        for (int i = 0; i < argc; i++) {
            
            printf(" %s", argv[i]);
        }
        #endif
        return 1;
    }
    
    #ifndef HELLOPNG_SILENCE
    printf("Inspecting file: %s\n", argv[1]);
    #endif
    
    DecodedImage decoded_png = read_png_from_disk(argv[1]);
    
    #ifndef HELLOPNG_SILENCE 
    printf(
        "finished decode_PNG, result was: %s\n",
        decoded_png.good ? "SUCCESS" : "FAILURE");
    printf(
        "rgba values in image: %u\n",
        decoded_png.rgba_values_size);
    printf(
        "pixels in image (info from image header): %u\n",
        decoded_png.pixel_count);
    printf(
        "image width: %u\n",
        decoded_png.width);
    printf(
        "image height: %u\n",
        decoded_png.height);
    #endif
    
    assert(
        decoded_png.width * decoded_png.height * 4 ==
            decoded_png.rgba_values_size); 
    
    if (decoded_png.good) {
        uint32_t avg_r = 0;
        uint32_t avg_g = 0;
        uint32_t avg_b = 0;
        uint32_t avg_alpha = 0;
        for (
            uint32_t i = 0;
            (i+3) < decoded_png.rgba_values_size;
            i += 4)
        {
            avg_r += decoded_png.rgba_values[i];
            avg_g += decoded_png.rgba_values[i+1];
            avg_b += decoded_png.rgba_values[i+2];
            avg_alpha += decoded_png.rgba_values[i+3];
        }
        #ifndef HELLOPNG_SILENCE
        printf(
            "average pixel: [%u,%u,%u,%u]\n",
            avg_r / decoded_png.pixel_count,
            avg_g / decoded_png.pixel_count,
            avg_b / decoded_png.pixel_count,
            avg_alpha / decoded_png.pixel_count);
        #endif
        
        #ifndef HELLOPNG_SILENCE
        // let's print the PNG to the screen in crappy
        // unicode characters
        uint8_t * printfeed = decoded_png.rgba_values;
        for (uint32_t h = 0; h < decoded_png.height; h++) {
            printf("\n");
            for (uint32_t w = 0; w < decoded_png.width; w++) {
                // average of rgb
                uint32_t pixel_strength =
                    (printfeed[0] + printfeed[1] + printfeed[2])
                        / 3;
                // apply alpha as brigthness
                pixel_strength =
                    (pixel_strength * printfeed[3]) / 255;
                
                if (pixel_strength < 30) {
                    printf("  "); 
                } else if (pixel_strength < 70) {
                    printf("░░");
                } else if (pixel_strength < 110) {
                    printf("▒▒");
                } else if (pixel_strength < 150) {
                    printf("▓▓");
                } else {
                    printf("██");
                }
                
                printfeed += 4;
            }
        }
        #endif
    }
    
    return 0;
}

