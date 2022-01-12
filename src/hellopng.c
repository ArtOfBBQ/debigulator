#include "png.h"

int main(int argc, const char * argv[]) 
{
    if (argc != 2) {
        printf("Please supply 1 argument (png file name)\n");
        printf("Got:");
        for (int i = 0; i < argc; i++) {
            
            printf(" %s", argv[i]);
        }
        return 1;
    }
    
    printf("Inspecting file: %s\n", argv[1]);
    
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
    
    printf("bytes read from raw file: %zu\n", bytes_read);
    fclose(imgfile);
    assert(bytes_read == fsize);
    
    for (int i = 0; i < 20; i++) {
        
        uint8_t * buffer_copy = start_of_buffer;
        printf("starting decode_PNG (run %u)...\n", i);
        DecodedPNG * decoded_png =
            decode_PNG(
                /* compressed_bytes: */ buffer_copy,
                /* compressed_bytes_size: */ bytes_read);
        
        printf(
            "finished decode_PNG, result was: %s\n",
            decoded_png->good ? "SUCCESS" : "FAILURE");
        printf(
            "bytes in image: %u (about %u pixels)\n",
            decoded_png->pixel_count,
            decoded_png->pixel_count / 4);
        printf(
            "image width: %u\n",
            decoded_png->width);
        printf(
            "image height: %u\n",
            decoded_png->height);
        
        free(decoded_png->pixels);
        free(decoded_png);
    }
    
    free(buffer);
    
    return 0;
}

