#include "png.h"
#include "stdio.h"

// #define HELLOPNG_SILENCE

int main(int argc, const char * argv[]) 
{
    if (argc != 2) {
        #ifndef HELLOPNG_SILENCE
        printf("Please supply 1 argument (png file name)\n");
        printf("Got:");
        for (int i = 0; i < argc; i++) {
            
            printf(" %s", argv[i]);
        }
        #endif
        return 1;
    }
    
    #ifndef HELLOPNG_SILENCE
    printf("Inspecting file: %s\n", argv[1]);
    #endif
    
    FILE * imgfile = fopen(
        argv[1],
        "rb");
    fseek(imgfile, 0, SEEK_END);
    size_t fsize = ftell(imgfile);
    fseek(imgfile, 0, SEEK_SET);
    
    uint8_t * buffer = malloc(fsize);
    uint8_t * start_of_buffer = buffer;
    
    size_t bytes_read = fread(
        /* ptr: */
            buffer,
        /* size of each element to be read: */
            1,
        /* nmemb (no of members) to read: */
            fsize,
        /* stream: */
            imgfile);
    
    #ifndef HELLOPNG_SILENCE
    printf("bytes read from raw file: %zu\n", bytes_read);
    #endif
    
    fclose(imgfile);
    if (bytes_read != fsize) {
        #ifndef HELLOPNG_SILENCE
        printf("Error - expected bytes read equal to fsize\n");
        #endif
        return 1;
    }
    
    for (int i = 0; i < 1; i++) {
        
        uint8_t * buffer_copy = start_of_buffer;
        #ifndef HELLOPNG_SILENCE
        printf("starting decode_PNG (run %u)...\n", i+1);
        #endif
        
        DecodedPNG * decoded_png =
            decode_PNG(
                /* compressed_bytes: */ buffer_copy,
                /* compressed_bytes_size: */ bytes_read);
       
        #ifndef HELLOPNG_SILENCE 
        printf(
            "finished decode_PNG, result was: %s\n",
            decoded_png->good ? "SUCCESS" : "FAILURE");
        printf(
            "rgba values in image: %u\n",
            decoded_png->rgba_values_size);
        printf(
            "pixels in image (info from image header): %u\n",
            decoded_png->pixel_count);
        printf(
            "image width: %u\n",
            decoded_png->width);
        printf(
            "image height: %u\n",
            decoded_png->height);
        #endif

	// let's convert to greyscale
	uint8_t * grayscale_vals = malloc(decoded_png->pixel_count);
	uint8_t * grayscale_vals_at = grayscale_vals;
	for (int i = 0; i < decoded_png->pixel_count; i += 4) {
	    
	    *grayscale_vals_at =
		(decoded_png->rgba_values[i]
		+ decoded_png->rgba_values[i + 1]
		+ decoded_png->rgba_values[i + 2])
		    / 3;

	    // apply alpha
	    *grayscale_vals_at++ *= (decoded_png->rgba_values[i + 3] / 255);
	}

	unsigned int pixels_per_char = decoded_png->width / 30;
	printf(
	    "printing grayscale in 30 chars, pixels/char: %u\n",
	    pixels_per_char);
	grayscale_vals_at = grayscale_vals;
       	
	for (
            int h = 0;
            h < decoded_png->height;
            h += pixels_per_char)
        {
	    printf("\n");
	    for (
                int w = 0;
                w < decoded_png->width;
                w += pixels_per_char)
            {
		unsigned int val_to_print = 0;
		int index = (h * decoded_png->width);
		index += w;
		for (int i = 0; i < pixels_per_char; i++) {
		    
		    index += i;
		    val_to_print += grayscale_vals[index];
		}
		val_to_print /= pixels_per_char;
                
		if (val_to_print < 10) {
		    printf(" ");
		} else if (val_to_print < 50) {
		    printf(".");
		} else if (val_to_print < 80) {
		    printf(":");
		} else if (val_to_print < 110) {
		    printf("░");
		} else if (val_to_print < 160) {
		    printf("▒");
		} else if (val_to_print < 210) {
		    printf("▓");
		} else {
		    printf("█");
		}
	    }
	}
	
        free(decoded_png->rgba_values);
        free(decoded_png);
    }
    
    free(buffer);
    
    return 0;
}

