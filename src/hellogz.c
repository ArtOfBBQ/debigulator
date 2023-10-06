/*
This file is meant as an example for how to
decompress & read a .gz file's contents using
"decode_gz.h"
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "decode_gz.h"


int main(int argc, const char * argv[])
{
    printf("Hello .gz files!\n");
    
    const char * file_to_open = NULL;
    if (argc != 2) {
        printf("Got no arguments, assuming 'gzipsample.gz'\n");
        file_to_open = "gzipsample.gz";
    } else {
        file_to_open = argv[1];
    }
    
    printf("Inspecting file: %s\n", file_to_open);
    
    FILE * gzipfile = fopen(
        file_to_open,
        "rb");
    
    if (gzipfile == NULL) {
        printf("no such file, exiting\n");
        return 1;
    }
    
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
    
    init_decode_gz(malloc);
    
    printf("contents: %s\n", (char *)buffer);
    
    DecodedData * decompressed_contents = NULL;
    for (uint32_t _ = 0; _ < 2000; _++) {
        
        decompressed_contents = decode_gz(
            /* buffer : */ buffer,
            /* buffer_size: */ (uint32_t)fsize);
        
        if (decompressed_contents == NULL) {
            printf("decode_gz() returned NULL, exiting...\n");
            return 1;
        }
    }
    
    printf("decompressed_contents contained:\n");
    printf("%s\n", decompressed_contents->data);
    
    free(buffer);
    
    return 0;
}
