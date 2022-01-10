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

    printf("starting decode_PNG...\n");
    DecodedPNG * decoded_png = decode_PNG(
        /* compressed_bytes: */ buffer,
        /* compressed_bytes_size: */ bytes_read);
    free(buffer);
    
    printf("finished decode_PNG.\n");
    printf(
        "result was: %s\n",
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
    
    return 0;
}

