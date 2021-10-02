#include "stdio.h"
#include "stdlib.h"

/*
BITMAP (*.bmp) File header

first 2 bytes are character identifiers such as:
BM
    Windows 3.1x, 95, NT, ... etc.
BA
    OS/2 struct bitmap array
CI
    OS/2 struct color icon
CP
    OS/2 const color pointer
IC
    OS/2 struct icon
PT
    OS/2 pointer

next 4 (bytes 3 to 6): 
    Size of BMP file in bytes.

next 2 (bytes 7 & 8):
    Reserved; actual value depends on the application
    that creates the image, if created manually can be 0 

next 2 (bytes 9 & 10):
    Reserved; actual value depends on the application
    that creates the image, if created manually can be 0 

next 4 (bytes 11 to 14):
    The offset, i.e. starting address, of the byte where
    the bitmap image data (pixel array) can be found.
*/

#pragma pack(push, 1)
typedef struct BMPHeader {
    char       file_type[2];
    uint32_t   file_size;
    char       reserved[4];
    uint32_t   bitmap_offset;
} BMPHeader;
#pragma pack(pop)

int main(int argc, const char * argv[]) 
{
    printf(
        "%s",
        "Hello world of bitmap (.BMP) files!\n");
    
    FILE * imgfile = fopen(
        "fs_angrymob.bmp",
        "rb");
    
    struct BMPHeader * my_header = malloc(sizeof(BMPHeader));
    
    if (imgfile == NULL) {
        printf(
            "%s",
            "FAILURE: file pointer was nullptr!\n");
        
        return 0;
    } else {
        
        printf(
            "%s",
            "file was opened succesfully!\n");
        int bytes_read = fread(
            /* ptr: */ my_header,
            /* size of each element to be read: */ 1,
            /* nmemb (no of members) to read: */ sizeof(BMPHeader),
            /* stream: */ imgfile);
        printf("read %d bytes succesfully\n", bytes_read);

        if (bytes_read != 14) {
            printf("%s", "aborting - expected 14 bytes");
            return 1;
        }
    }
    
    printf(
        "header file type size in bytes: %lu\n",
        sizeof(my_header->file_type));
    printf(
        "header has file type: %c%c\n",
        my_header->file_type[0],
        my_header->file_type[1]);
    printf(
        "header has file size: %u\n",
        my_header->file_size);
    printf(
        "reserved characters read: %s\n", my_header->reserved);
    printf(
        "bitmap offset: %u\n", my_header->bitmap_offset);
    fclose(imgfile);
    
    return 0;
}

