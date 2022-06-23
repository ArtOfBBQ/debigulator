/*
This file is meant as an example of how to use the "png.h"
header to read pixels from a .png file.
*/

#include "decode_png.h"
#include "decodedimage.h"
#include "stdio.h"

#include <Foundation/foundation.h>

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

static DecodedImage read_png_from_disk(
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
    
    uint32_t png_width;
    uint32_t png_height;
    uint32_t width_height_check_success = 0;
    get_PNG_width_height(
        /* uint8_t * compressed_bytes: */
            (uint8_t *)imgfile.contents,
        /* uint32_t compressed_bytes_size: */
            bytes_read,
        /* uint32_t * width_out: */
            &png_width,
        /* uint32_t * height_out: */
            &png_height,
        /* good: */
            &width_height_check_success);
    
    if (!width_height_check_success) {
        #ifndef HELLOPNG_SILENCE
        printf(
            "Failed to extract width/height from PNG file\n");
        #endif
        assert(0);
    }
    
    assert(png_width > 0);
    assert(png_height > 0);
    
    return_value.rgba_values_size =
        png_width * png_height * 4;
    return_value.rgba_values =
        (uint8_t *)malloc(return_value.rgba_values_size);
    
    // assert(buffer_copy == start_of_buffer);
    decode_PNG(
        /* compressed_input: */
            (uint8_t *)imgfile.contents,
        /* compressed_input_size: */
            bytes_read,
        /* out_rgba_values: */
            return_value.rgba_values,
        /* out_rgba_values_size: */
            return_value.rgba_values_size,
        /* out_good: */
            &return_value.good);
    return_value.width = png_width;
    return_value.height = png_height;
    
    free(imgfile.contents);
    
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

    /*
    // TODO: remove this test code focusing on only 1 file
    #define FILENAMES_CAP 1
    char * filenames[FILENAMES_CAP] = {
        //(char *)"structuredart2.png"
        (char *)"font.png"
    };
    */
    
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
        
        assert(
            decoded_images[filename_i].width
                * decoded_images[filename_i].height * 4 ==
                    decoded_images[filename_i].rgba_values_size); 
    }
    
    return 0;
}

