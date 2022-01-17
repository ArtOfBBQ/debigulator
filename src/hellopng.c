#include "png.h"
#include "stdio.h"
#include "assert.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// #define HELLOPNG_SILENCE

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
    
    FILE * imgfile = fopen(
        argv[1],
        "rb");
    fseek(imgfile, 0, SEEK_END);
    size_t fsize = ftell(imgfile);
    fseek(imgfile, 0, SEEK_SET);
    
    uint8_t * buffer = malloc(fsize);
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
    printf("bytes read from raw file: %zu\n", bytes_read);
    #endif
    
    fclose(imgfile);
    if (bytes_read != fsize) {
        #ifndef HELLOPNG_SILENCE
        printf("Error - expected bytes read equal to fsize\n");
        #endif
        return 1;
    }
    
    uint8_t * buffer_copy = start_of_buffer;
    
    DecodedPNG * decoded_png =
        decode_PNG(
            /* compressed_bytes: */ buffer_copy,
            /* compressed_bytes_size: */ bytes_read);
   
    #ifndef HELLOPNG_SILENCE 
    printf(
        "finished decode_PNG, result was: %s\n",
        decoded_png->good ? "SUCCESS" : "FAILURE");
    printf(
        "rgba values in image: %u\n",
        decoded_png->rgba_values_size);
    printf(
        "pixels in image (info from image header): %u\n",
        decoded_png->pixel_count);
    printf(
        "image width: %u\n",
        decoded_png->width);
    printf(
        "image height: %u\n",
        decoded_png->height);
    #endif
   
    // let's write our image to disk with an existing, working
    // library to see if it's correct
    int stb_success = stbi_write_bmp(
        /* filename        : */ "output.bmp",
        /* width           : */ decoded_png->width,
        /* height          : */ decoded_png->height,
        /* comp            : */ 4,
        /* data            : */ (void *)decoded_png->rgba_values);
    printf(
        "stbi_write_bmp was %s\n",
        stb_success ? "successful" : "failed");
    
    free(decoded_png->rgba_values);
    free(decoded_png);
    free(buffer);
    
    return 0;
}

