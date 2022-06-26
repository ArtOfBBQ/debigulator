/*
This file is meant as an example of how to use the "png.h"
header to read pixels from a .png file.
*/

#define WRITING_VERSION

#include "decode_png.h"
#include "decodedimage.h"
#include "stdio.h"

#ifdef WRITING_VERSION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_write.h"
#endif

#include <Foundation/foundation.h>

// 100mb ->                     1..000...
#define APPLICATION_MEMORY_SIZE 100000000
uint8_t * memory_store = (uint8_t *)malloc(APPLICATION_MEMORY_SIZE);
uint64_t memory_store_remaining = APPLICATION_MEMORY_SIZE;

// #define HELLOPNG_SILENCE

typedef struct FileBuffer {
    uint64_t size;
    char * contents;
} FileBuffer;

/*
Get a file's size. Returns -1 if no such file
*/
static int64_t platform_get_filesize(const char * filename)
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
    
    decode_PNG(
        /* compressed_input: */
            (uint8_t *)imgfile.contents,
        /* compressed_input_size: */
            bytes_read,
        /* receiving_memory_store: */
            memory_store,
        /* receiving_memory_store_size: */
            memory_store_remaining,
        /* out_rgba_values_size: */
            &(return_value.rgba_values_size),
        /* out_height: */
            &return_value.height,
        /* out_width: */
            &return_value.width,
        /* out_good: */
            &return_value.good);
    return_value.rgba_values = memory_store;
    memory_store += return_value.rgba_values_size;
    memory_store_remaining -= return_value.rgba_values_size;
    
    free(imgfile.contents);
    
    if (return_value.good) {
        return_value.pixel_count =
            return_value.width * return_value.height;
        memory_store += return_value.rgba_values_size;
        memory_store_remaining -= return_value.rgba_values_size;
        printf(
            "memory store remaining: %llu bytes\n",
            memory_store_remaining);
    }
    
    return return_value;
}

int main(int argc, const char * argv[]) 
{
    #ifndef HELLOPNG_SILENCE 
    printf(
        "Starting hellopng...\n");
    #endif
    
    #define FILENAMES_CAP 14
    char * filenames[FILENAMES_CAP] = {
        (char *)"fs_angrymob.png",
        (char *)"backgrounddetailed1.png",
        (char *)"font.png",
        (char *)"fs_birdmystic.png",
        (char *)"fs_bribery.png",
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
        printf("created filename: %s\n", out_filename);
        
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
    
    return 0;
}

