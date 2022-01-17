#include "png.h"
#include "stdio.h"
#include "assert.h"

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
    
    for (int i = 0; i < 1; i++) {
        
        uint8_t * buffer_copy = start_of_buffer;
        #ifndef HELLOPNG_SILENCE
        printf("starting decode_PNG (run %u)...\n", i+1);
        #endif
        
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
       
        for (
            int i = 0;
            i < decoded_png->rgba_values_size;
            i += 4)
        {
            if (i % 32 == 0) { printf("\n\n"); }
            printf(
                "[%u,%u,%u,%u],",
                decoded_png->rgba_values[i],
                decoded_png->rgba_values[i + 1],
                decoded_png->rgba_values[i + 2],
                decoded_png->rgba_values[i + 3]);
        }
	
        free(decoded_png->rgba_values);
        free(decoded_png);
    }
    
    free(buffer);
    
    return 0;
}

