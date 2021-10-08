#include "stdio.h"
#include "stdlib.h"
#include "assert.h"

// This is expected at the start of every .bmp file
#pragma pack(push, 1)
typedef struct BMPHeader {
    char       file_type[2];
    uint32_t   file_size;
    char       reserved[4];
    uint32_t   bitmap_offset;
} BMPHeader;

// There are many .bmp variants, but I'm guessing this is
// used in 99% of cases based on documentation.
// If you want to read .bmp files in a robust way I guess
// you must be prepared for all formats, it seems like a
// daunting task.
// For our purposes this data could just be added to the
// previous struct as one big header, but I think a robust
// parser would have to look at the first header first,
// and then - depending on the file format described there -
// choose how to proceed.
typedef struct BitmapInfoHeader {
    uint32_t   header_size;
    uint32_t   width;
    uint32_t   height;
    uint16_t   color_planes;
    uint16_t   bits_per_pixel;
    uint32_t   compression_method;
    uint32_t   raw_bitmap_size; // same as width * height * 4
    uint32_t   horizontal_resolution;
    uint32_t   vertical_resolution;
    uint32_t   colors_in_palette;
    uint32_t   num_important_colors_used;
} BitmapInfoHeader;

/*
Each pixel is stored as a 32-bit [RGBA] value with each
value ranging from 0 to 255. The first pixel in the file
is actually the BOTTOM LEFT pixel, which surprised me,
then you move right along the bottom row, then up to
the 2nd lowest column. I haven't noticed any kind of compression
so far, so bitmaps may be the easiest type of image to parse
*/
typedef struct Pixel {
    uint8_t Red;
    uint8_t Green;
    uint8_t Blue;
    uint8_t Alpha;
} Pixel;
#pragma pack(pop)


int main(int argc, const char * argv[]) 
{
    printf(
        "%s\n",
        "Hello .bmp files!");
    
    if (argc != 2) {
        printf("Please supply 1 argument (png file name)\n");
        printf("Got:");
        for (int i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        return 1;
    }

    printf("Analyzing file: %s\n", argv[1]);
    FILE * imgfile = fopen(
        argv[1],
        "rb");
    
    BMPHeader * my_header = malloc(sizeof(BMPHeader));
    uint32_t bytes_read = 0;
    
    if (imgfile == NULL) {
        printf(
            "%s",
            "FAILURE: file pointer was nullptr!\n");
        
        return 0;
    } else {
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
        /* stream: */
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
    
    uint32_t pixel_data_len = 
        bm_header->width * bm_header->height;
    printf("allocate memory for %u pixels...\n", pixel_data_len);
    Pixel * pixel_data =
        (Pixel *)malloc(
            pixel_data_len * sizeof(Pixel));
    
    fseek(
        imgfile,
        my_header->bitmap_offset,
        SEEK_SET); 
    
    bytes_read = fread(
        /* ptr: */
            pixel_data,
        /* size of each element to be read: */
            1,
        /* nmemb (no of members) to read: */
            pixel_data_len * sizeof(Pixel),
        /* stream: */
            imgfile);
    
    printf("read %d bytes of pixel data\n", bytes_read);
    
    fclose(imgfile); 
    
    uint32_t final_i = pixel_data_len; 
    for (
        uint32_t i = 0;
        i < final_i;
        i++)
    {
        if (pixel_data[i].Alpha
            || pixel_data[i].Red
            || pixel_data[i].Green
            || pixel_data[i].Blue)
        {
            printf(
                "pixel %u [RGBA] was: [%u,%u,%u,%u]\n",
                i,
                pixel_data[i].Red,
                pixel_data[i].Green,
                pixel_data[i].Blue,
                pixel_data[i].Alpha);
            
            // this is just to avoid flooding the screen
            // for normal-sized bitmaps
            if (i + 100 < final_i) {
                final_i = i + 40;
            }
        }
    }
    
    return 0;
}

