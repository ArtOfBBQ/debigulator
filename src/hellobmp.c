/*
This file is meant as an example of how to use the "bmp.h"
header to read pixels from a .bmp file.
*/

#include "decode_bmp.h"
#include "stdio.h"
#include "stdlib.h"

#define true 1
#define false 0

#define WRITING_VERSION

// 50mb ->                      50...000
#define APPLICATION_MEMORY_SIZE 50000000
static uint8_t * memory_store;
static uint64_t memory_store_remaining = APPLICATION_MEMORY_SIZE;

static void align_memory() {
    while ((uintptr_t)(void *)memory_store % 16 != 0) {
        assert(memory_store_remaining >= 1);
        memory_store += 1;
        memory_store_remaining -= 1;
    }
    
    #ifndef INFLATE_IGNORE_ASSERTS
    assert((uintptr_t)(void *)memory_store % 16 == 0);
    #endif
}

typedef struct FileBuffer {
    uint64_t size;
    char * contents;
} FileBuffer;

static void filename_to_filepath(
    const char * filename,
    char * out_filename)
{
    char * app_path = (char *)".";
    
    uint32_t i = 0;
    while (app_path[i] != '\0') {
        out_filename[i] = app_path[i];
        i++;
    }
    out_filename[i++] = '/';
    uint32_t j = 0;
    while (filename[j] != '\0') {
        out_filename[i++] = filename[j++];
    }
    out_filename[i++] = '\0';
}

static uint64_t platform_get_filesize(
    const char * filename)
{
    char path_and_filename[1000];
    filename_to_filepath(
        /* filename: */ filename,
        /* char * out_filename: */ path_and_filename);
    
    FILE * file_handle = fopen(
        path_and_filename,
        "rb+");
    
    if (!file_handle) {
        assert(0);
        return -1;
    }
    
    fseek(file_handle, 0L, SEEK_END);
    int64_t fsize = ftell(file_handle);
    
    fclose(file_handle);
    
    return fsize;
}

static void platform_read_file(
    const char * filename,
    char * out_preallocatedbuffer,
    const uint64_t out_size)
{
    char path_and_filename[1000];
    filename_to_filepath(
        /* filename: */ filename,
        /* char * out_filename: */ path_and_filename);
    
    FILE * file_handle = fopen(
        path_and_filename,
        "rb+");

    assert(file_handle);
    
    fread(
        /* ptr: */
            out_preallocatedbuffer,
        /* size of each element to be read: */
            1,
        /* nmemb (no of members) to read: */
            out_size - 1,
        /* stream: */
            file_handle);
    
    fclose(file_handle);
    
    out_preallocatedbuffer[out_size - 1] = '\0'; // for windows
}

static void platform_write_file(
    const char * filename,
    unsigned char * to_write,
    const uint64_t out_size)
{
    char path_and_filename[1000];
    filename_to_filepath(
        /* filename: */ filename,
        /* char * out_filename: */ path_and_filename);
    
    FILE * file_handle = fopen(
        path_and_filename,
        "wb+");
    
    fwrite(
        to_write,
        out_size,
        1,
        file_handle);
    
    fclose(file_handle);
}

typedef struct Image {
    uint8_t * rgba_values;
    uint64_t rgba_values_size;
    uint32_t width;
    uint32_t height;
    uint32_t good;
} Image;

Image read_bmp_from_disk(
    const char * filename)
{
    Image return_value;
    return_value.good = 0;
    
    FileBuffer imgfile;
    imgfile.size = platform_get_filesize(filename);
    imgfile.contents = (char *)malloc(sizeof(char) * imgfile.size);
    
    platform_read_file(
        /* const char * filename: */
            filename,
        /* char * out_preallocatedbuffer: */
            imgfile.contents,
        /* uint64_t out_size: */
            imgfile.size);
    int64_t bytes_read = imgfile.size;
    
    
    get_BMP_width_height(
        /* const uint8_t * raw_input: */
            (uint8_t *)imgfile.contents,
        /* const uint64_t raw_input_size: */
            bytes_read,
        /* uint32_t * out_width: */
            &return_value.width,
        /* uint32_t * out_height: */
            &return_value.height,
        /* uint32_t * out_good: */
            &return_value.good);
    
    assert(return_value.width > 0);
    assert(return_value.height > 0);
    
    align_memory();
    return_value.rgba_values = (uint8_t *)memory_store;
    return_value.rgba_values_size =
        return_value.width * return_value.height * 4;
    assert(memory_store_remaining >= return_value.rgba_values_size);
    memory_store += return_value.rgba_values_size;
    memory_store_remaining -= return_value.rgba_values_size;  
    
    assert(return_value.good);
    return_value.good = false;
    
    decode_BMP(
        /* raw_input: */
            (uint8_t *)imgfile.contents,
        /* raw_input_size: */
            bytes_read,
        /* out_rgba_values: */
            return_value.rgba_values,
        /* out_rgba_values_size: */
            return_value.rgba_values_size,
        /* out_good: */
            &return_value.good);
    
    assert(return_value.good);
    
    return return_value;
}

int main(int argc, const char * argv[]) {
    
    // 990mb ->                      99.000...
    memory_store = (uint8_t *)malloc(APPLICATION_MEMORY_SIZE);
    
    #ifndef HELLOBMP_SILENCE 
    printf("Starting hellobmp...\n");
    #endif
    
    #define FILENAMES_CAP 2
    char * filenames[FILENAMES_CAP] = {
        (char *)"fs_psychologist.bmp",
        (char *)"fs_fightingpit.bmp"
    };
    
    Image decoded_images[FILENAMES_CAP];
    
    for (
        uint32_t filename_i = 0;
        filename_i < FILENAMES_CAP;
        filename_i++)
    {
        decoded_images[filename_i] = read_bmp_from_disk(filenames[filename_i]);
        
        #ifndef HELLOBMP_SILENCE 
        printf(
            "finished decode_BMP for %s\n",
            filenames[filename_i]);
        #endif
    }
    
    #ifdef WRITING_VERSION
    char * out_filename = (char *)malloc(30);
    out_filename[0] = 'o';
    out_filename[1] = 'u';
    out_filename[2] = 't';
    out_filename[3] = '\0';
    
    for (uint32_t i = 0; i < FILENAMES_CAP; i++) {
        uint32_t char_i = 3;
        while (filenames[i][char_i - 3] != '\0') {
            out_filename[char_i] = filenames[i][char_i - 3];
            char_i++;
        }
        out_filename[char_i] = '\0';
        
        if (!decoded_images[i].good) {
            continue;
        }
        assert(decoded_images[i].rgba_values_size > 0);
        assert(decoded_images[i].rgba_values_size >=
            decoded_images[i].width * decoded_images[i].height * 4);
        
        char * encoded_bmp = (char *)memory_store;
        uint64_t encoded_bmp_size = 
            (decoded_images[i].width * decoded_images[i].height * 4) + 55;
        memory_store += encoded_bmp_size;
        encode_BMP(
            /* const uint8_t * rgba: */
                decoded_images[i].rgba_values,
            /* const uint64_t rgba_size: */
                decoded_images[i].rgba_values_size,
            /* const uint32_t width: */
                decoded_images[i].width,
            /* const uint32_t height: */
                decoded_images[i].height,
            /* char * recipient: */
                encoded_bmp,
            /* const int64_t recipient_capacity: */
                encoded_bmp_size);
        
        platform_write_file(
            /* const char * filename: */
                out_filename,
            /* unsigned char * to_write: */
                (unsigned char *)encoded_bmp,
            /* const uint64_t out_size: */
                encoded_bmp_size);
    }
    #endif
    
    return 0;
}

