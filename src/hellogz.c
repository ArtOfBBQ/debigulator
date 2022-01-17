/*
This file is meant as an example for how to
decompress & read a .gz file's contents using
"decode_gz.h"
*/

#include "decode_gz.h"


int main(int argc, const char * argv[])
{
    printf("Hello .gz files!\n");
    
    if (argc != 2) {
        printf("Please supply 1 argument : the PNG file name.\n");
        for (int i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        return 1;
    }
    
    printf("Inspecting file: %s\n", argv[1]);
    
    FILE * gzipfile = fopen(
        argv[1],
        "rb");
    fseek(gzipfile, 0, SEEK_END);
    size_t fsize = ftell(gzipfile);
    fseek(gzipfile, 0, SEEK_SET);
    
    printf("creating buffer..\n");
    
    uint8_t * buffer = malloc(fsize);
    
    size_t bytes_read = fread(
        /* ptr: */
            buffer,
        /* size of each element to be read: */
            1, 
        /* nmemb (no of members) to read: */
            fsize,
        /* stream: */
            gzipfile);
    printf("bytes read: %zu\n", bytes_read);
    fclose(gzipfile);
    assert(bytes_read == fsize);
    
    char * decompressed_contents = decode_gz(
        /* buffer : */ buffer,
        /* buffer_size: */ fsize);
    
    printf("decompressed_contents contained:\n");
    printf("%s\n", decompressed_contents);
    
    return 0;
}

