#include "stdio.h"
#include "stdlib.h"

// This is expected at the start of every .bmp file
#pragma pack(push, 1)
typedef struct BMPHeader {
    char       file_type[2];
    uint32_t   file_size;
    char       reserved[4];
    uint32_t   bitmap_offset;
} BMPHeader;
#pragma pack(pop)

// There are many .bmp variants, but I'm guessing this is
// used in 99% of cases based on documentation.
// If you want to read .bmp files in a robust way I guess
// you must be prepared for all formats, it seems like a
// daunting task.
#pragma pack(push, 1)
typedef struct BitmapInfoHeader {
    uint32_t   header_size;
    uint32_t   width;
    uint32_t   height;
    uint16_t   color_planes;
    uint16_t   bits_per_pixel;
    uint32_t   compression_method;
    uint32_t   raw_bitmap_size;
    uint32_t   horizontal_resolution;
    uint32_t   vertical_resolution;
    uint32_t   colors_in_palette;
    uint32_t   num_important_colors_used;
} BitmapInfoHeader;
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
    int bytes_read = 0;
    
    if (imgfile == NULL) {
        printf(
            "%s",
            "FAILURE: file pointer was nullptr!\n");
        
        return 0;
    } else {
        
        printf(
            "%s",
            "file was opened succesfully!\n");
        bytes_read = fread(
            /* ptr: */
                my_header,
            /* size of each element to be read: */
                1,
            /* nmemb (no of members) to read: */
                sizeof(BMPHeader),
            /* stream: */
                imgfile);
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
    
    BitmapInfoHeader * bm_header =
        malloc(sizeof(BitmapInfoHeader));
    
    bytes_read = fread(
        /* ptr: */
            bm_header,
        /* size of each element to be read: */
            1,
        /* nmemb (no of members) to read: */
            sizeof(BitmapInfoHeader),
        /* stream
            (imgfile pointer is already advanced by
            previous call to fread): */
            imgfile);
    
    if (bytes_read != 40) {
       printf(
            "%s %d\n",
            "error! expected 40 bytes BitmapInfoHeader, got:",
            bytes_read);
        return 1;
    } else {
        printf(
            "Succesfully read %d byte InfoBitmapHeader struct\n",
            bytes_read);
        printf(
            "bm_header->header_size: %d\n",
            bm_header->header_size);
        printf(
            "bm_header->width: %d\n",
            bm_header->width);
        printf(
            "bm_header->height: %d\n",
            bm_header->height);
        printf(
            "bm_header->color_planes: %d\n",
            bm_header->color_planes);
        printf(
            "bm_header->bits_per_pixel: %d\n",
            bm_header->bits_per_pixel);
        printf(
            "bm_header->compression_method: %d\n",
            bm_header->compression_method);
        printf(
            "bm_header->raw_bitmap_size: %d\n",
            bm_header->raw_bitmap_size);
        printf(
            "bm_header->horizontal_resolution: %d\n",
            bm_header->horizontal_resolution);
        printf(
            "bm_header->vertical_resolution: %d\n",
            bm_header->vertical_resolution);
        printf(
            "bm_header->colors_in_palette: %d\n",
            bm_header->colors_in_palette);
        printf(
            "bm_header->num_important_colors_used: %d\n",
            bm_header->num_important_colors_used);
    }
    
    fclose(imgfile);
    
    return 0;
}

