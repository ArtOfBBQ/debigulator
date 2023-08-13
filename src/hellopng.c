/*
This file is meant as an example of how to use the "png.h"
header to read pixels from a .png file.
*/

#define WRITING_VERSION

#include "decode_png.h"
#include "stdio.h"
#include <time.h>

#ifdef WRITING_VERSION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_write.h"
#endif

typedef struct FileBuffer {
    uint64_t size;
    char * contents;
} FileBuffer;

static void filename_to_filepath(
    const char * filename,
    char * out_filename)
{
    assert(filename != NULL);
    assert(filename[0] != '\0');
    assert(out_filename != NULL);
    
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

static uint8_t * dpng_working_memory = NULL;
static uint32_t dpng_working_memory_size = 0;

typedef struct Image {
    uint8_t * rgba_values;
    uint64_t rgba_values_size;
    uint32_t width;
    uint32_t height;
    uint32_t good;
} Image;

Image read_png_from_disk(
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
    
    get_PNG_width_height(
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
    
    return_value.rgba_values_size =
        return_value.width * return_value.height * 4;
    return_value.rgba_values = (uint8_t *)malloc(return_value.rgba_values_size);
    
    assert(return_value.good);
    return_value.good = 0;
    
    decode_PNG(
        /* const uint8_t * compressed_input: */
            (uint8_t *)imgfile.contents,
        /* const uint64_t compressed_input_size: */
            bytes_read,
        /* const uint8_t * out_rgba_values: */
            return_value.rgba_values,
        /* const uint64_t rgba_values_size: */
            return_value.rgba_values_size,
        /* out_good: */
            &return_value.good);
    
    assert(return_value.good);
    
    return return_value;
}

int main(int argc, const char * argv[]) 
{
    #ifndef HELLOPNG_SILENCE 
    printf(
        "Starting hellopng...\n");
    #endif
    
    init_PNG_decoder(/* void *(*malloc_funcptr)(size_t): */ malloc);
    
    #define FILENAMES_CAP 14
    char * filenames[FILENAMES_CAP] = {
        (char *)"backgrounddetailed1.png",
        (char *)"immunetomustsurvive.png",
    };
        
    Image decoded_images[FILENAMES_CAP];
    
    clock_t tic = clock();
    
    for (
        uint32_t filename_i = 0;
        filename_i < FILENAMES_CAP;
        filename_i++)
    {
        if (
            filenames[filename_i] == NULL ||
            filenames[filename_i][0] == '\0')
        {
            break;
        }
        
        decoded_images[filename_i] = read_png_from_disk(filenames[filename_i]);
        
        #ifndef HELLOPNG_SILENCE 
        printf(
            "finished decode_PNG for %s, result was: %s\n",
            filenames[filename_i],
            decoded_images[filename_i].good ?
                "SUCCESS" : "FAILURE");
        #endif
    }
    
    clock_t toc = clock();
    printf("Elapsed: %f seconds\n", (double)(toc - tic) / CLOCKS_PER_SEC);
    
    printf("write files with stb_write...\n");
    
    #ifdef WRITING_VERSION
    char * out_filename = (char *)malloc(30);
    out_filename[0] = 'o';
    out_filename[1] = 'u';
    out_filename[2] = 't';
    out_filename[3] = '\0';
    
    for (uint32_t i = 0; i < FILENAMES_CAP; i++) {
        if (
            filenames[i] == NULL ||
            filenames[i][0] == '\0')
        {
            break;
        }
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
        
        int result = stbi_write_png(
            /* char const *filename: */
                out_filename,
            /* int w: */
                decoded_images[i].width,
            /* int h: */
                decoded_images[i].height,
            /* int comp: */
                4,
            /* const void *data: */
                decoded_images[i].rgba_values,
            /* int stride_in_bytes: */
                decoded_images[i].width * 4);
        
        printf(
            "write to %s result: %i\n",
            out_filename,
            result);
    }
    #endif
    
    printf("end of hellopng\n");
    
    return 0;
}
