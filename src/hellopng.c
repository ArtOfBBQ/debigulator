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

#include <Foundation/foundation.h>

// 990mb ->                      99.000...
#define APPLICATION_MEMORY_SIZE  990000000
uint8_t * memory_store = (uint8_t *)malloc(APPLICATION_MEMORY_SIZE);
uint64_t memory_store_remaining = APPLICATION_MEMORY_SIZE;

// 30mb ->                        30...000
#define INFLATE_WORKING_MEMORY    30000000
uint8_t * inflate_working_memory = (uint8_t *)malloc(INFLATE_WORKING_MEMORY);
uint64_t inflate_working_memory_size = INFLATE_WORKING_MEMORY;

typedef struct DecodedImage {
    uint8_t * rgba_values;
    uint32_t rgba_values_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_count; // rgba_values_size / 4
    uint32_t good;
} DecodedImage;

// #define HELLOPNG_SILENCE

static void align_memory()
{
    while ((uintptr_t)(void *)memory_store % 16 != 0) {
        assert(memory_store_remaining >= 1);
        memory_store += 1;
        memory_store_remaining -= 1;
    }
    
    while ((uintptr_t)(void *)inflate_working_memory % 16 != 0) {
        assert(inflate_working_memory_size >= 1);
        inflate_working_memory += 1;
        inflate_working_memory_size -= 1;
    }
    
    #ifndef INFLATE_IGNORE_ASSERTS
    assert((uintptr_t)(void *)memory_store % 16 == 0);
    assert((uintptr_t)(void *)inflate_working_memory % 16 == 0);
    #endif
}

typedef struct FileBuffer {
    uint64_t size;
    char * contents;
} FileBuffer;

/*
Get a file's size. Returns -1 if no such file
*/
static int64_t platform_get_filesize(
    const char * filename)
{
    NSString * nsfilename = [NSString
        stringWithUTF8String:filename];
    
    NSURL * file_url = [[NSBundle mainBundle]
        URLForResource:[nsfilename stringByDeletingPathExtension]
        withExtension: [nsfilename pathExtension]];
        
    NSError * error_value = nil;
    NSNumber * file_size;
    
    [file_url
        getResourceValue:&file_size
        forKey:NSURLFileSizeKey
        error:&error_value];
    
    if (error_value != nil)
    {
        NSLog(@" error => %@ ", [error_value userInfo]);
        assert(0);
        return -1;
    }
    
    return file_size.intValue;
}

static void platform_read_file(
    const char * filename,
    FileBuffer * out_preallocatedbuffer)
{
    NSString * nsfilename = [NSString
        stringWithUTF8String:filename];
    
    NSURL * file_url = [[NSBundle mainBundle]
        URLForResource:[nsfilename stringByDeletingPathExtension]
        withExtension: [nsfilename pathExtension]];
    
    if (file_url == NULL) {
        printf("file_url nil!\n");
        assert(0);
    }
    
    NSError * error = NULL; 
    NSData * file_data =
        [NSData
            dataWithContentsOfURL:file_url
            options:NSDataReadingUncached
            error:&error];
    
    if (error != NULL) {
        printf("error while reading NSData from file_url\n");
        assert(0);
    }
    
    /*
    NSString * debug_string = [
        [NSString alloc]
            initWithData:file_data
            encoding:NSASCIIStringEncoding];
    */
    
    if (file_data == nil) {
        NSLog(
            @"error => %@ ",
            [error userInfo]);
        assert(0);
    }
    
    if (out_preallocatedbuffer->size >
        [file_data length])
    {
        out_preallocatedbuffer->size = [file_data length];
    }
    
    [file_data
        getBytes:out_preallocatedbuffer->contents
        range:NSMakeRange(0, out_preallocatedbuffer->size)];
        // length:out_preallocatedbuffer->size];
}

DecodedImage read_png_from_disk(
    const char * filename)
{
    DecodedImage return_value;
    return_value.good = 0;
    
    FileBuffer imgfile;
    imgfile.size = platform_get_filesize(filename);
    imgfile.contents = (char *)malloc(sizeof(char) * imgfile.size);
    
    platform_read_file(
        /* const char * filename: */
            filename,
        /* FileBuffer * out_preallocatedbuffer: */
            &imgfile);
    int64_t bytes_read = imgfile.size;
    
    get_PNG_width_height(
        /* const uint8_t * compressed_input: */
            (uint8_t *)imgfile.contents,
        /* const uint64_t compressed_input_size: */
            bytes_read,
        /* uint32_t * out_width: */
            &return_value.width,
        /* uint32_t * out_height: */
            &return_value.height,
        /* uint32_t * out_good: */
            &return_value.good);
    
    if (!return_value.good) {
        return return_value;
    }
    
    align_memory();
    return_value.rgba_values = (uint8_t *)memory_store;
    return_value.pixel_count = return_value.width * return_value.height;
    return_value.rgba_values_size = return_value.pixel_count * 4;
    assert(memory_store_remaining >= return_value.rgba_values_size);
    memory_store += return_value.rgba_values_size;
    memory_store_remaining -= return_value.rgba_values_size;  
    
    decode_PNG(
        /* compressed_input: */
            (uint8_t *)imgfile.contents,
        /* compressed_input_size: */
            bytes_read,
        /* out_rgba_values: */
            return_value.rgba_values,
        /* out_rgba_values_size: */
            return_value.rgba_values_size,
        /* inflate_working_memory: */
            inflate_working_memory,
        /* inflate_working_memory_size: */
            inflate_working_memory_size,
        /* out_good: */
            &return_value.good);
    
    // free(imgfile.contents);
        
    return return_value;
}

int main(int argc, const char * argv[]) 
{
    #ifndef HELLOPNG_SILENCE 
    printf(
        "Starting hellopng...\n");
    #endif
    
    #define FILENAMES_CAP 15
    char * filenames[FILENAMES_CAP] = {
        (char *)"fs_angrymob.png",
        (char *)"backgrounddetailed1.png",
        (char *)"font.png",
        (char *)"fs_birdmystic.png",
        (char *)"fs_bribery.png",
        (char *)"phoebus.png",
        (char *)"fs_bridge.png",
        (char *)"fs_cannon.png",
        (char *)"gimp_test.png",
        (char *)"purpleback.png",
        (char *)"receiver.png",
        (char *)"structuredart1.png",
        (char *)"structuredart1.png",
        (char *)"structuredart2.png",
        (char *)"structuredart3.png"
    };
        
    DecodedImage decoded_images[FILENAMES_CAP];
    
    
    clock_t tic = clock();
    
    for (
        uint32_t filename_i = 0;
        filename_i < FILENAMES_CAP;
        filename_i++)
    {
        decoded_images[filename_i] =
            read_png_from_disk(filenames[filename_i]);
        
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

