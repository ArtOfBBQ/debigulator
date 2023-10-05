#include "decode_gz.h"

static void * (* malloc_func)(size_t __size) = NULL;

void init_decode_gz(
    void * (* malloc_funcptr)(size_t __size))
{
    malloc_func = malloc_funcptr;
}

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

static char * consume_bytes(
    uint8_t ** buffer,
    uint32_t * buffer_size,
    uint32_t amount_to_consume)
{
    #ifndef DECODE_GZ_IGNORE_ASSERTS
    assert(**buffer_size >= amount_to_consume);
    #endif
    
    char * return_value = (char *)*buffer;
    *buffer += amount_to_consume;
    *buffer_size -= amount_to_consume;
    
    return return_value;
}
#define consume_struct(type, doubleptr_buffer, ptr_buffer_size) (type *)consume_bytes(doubleptr_buffer, ptr_buffer_size, sizeof(type))

static char * consume_till_terminate(
    uint8_t ** from,
    uint32_t * from_size,
    const uint32_t max_size,
    const char terminator)
{
    #ifndef DECODE_GZ_IGNORE_ASSERTS
    assert(max_size <= *from_size);
    #endif
    
    uint32_t string_size = 0;
    while (
        (*from)[string_size] != terminator
        && string_size < max_size)
    {
        string_size++;
    }
    
    #ifndef DECODE_GZ_IGNORE_ASSERTS
    assert(string_size > 0);
    #endif
    
    char * return_value = (char *)*from;
    *from += (string_size + 1);
    *from_size -= (string_size + 1);
    
    return return_value;
}

#pragma pack(push, 1)
/*
The specification says a gzip file must start with:
+---+---+---+---+---+---+---+---+---+---+
|ID1|ID2|CM |FLG|     MTIME     |XFL|OS | (more-->)
+---+---+---+---+---+---+---+---+---+---+
*/
typedef struct GZHeader {
    uint8_t id1; // must be 31 to be gzip
    uint8_t id2; // must be 139 to be gzip
    uint8_t CM; // CM = 8 denotes the "deflate" method
    uint8_t FLG;
    uint32_t MTIME;
    uint8_t XFLG;
    uint8_t OS;
} GZHeader;

/*
The specification says a gzip file must end with:
+---+---+---+---+---+---+---+---+
|     CRC32     |     ISIZE     |
+---+---+---+---+---+---+---+---+
*/
typedef struct GZFooter {
    uint32_t CRC32;
    uint32_t ISIZE;
} GZFooter;
#pragma pack(pop)

DecodedData * decode_gz(
    uint8_t * compressed_bytes,
    uint32_t compressed_bytes_left)
{
    if (malloc_func == NULL) {
        #ifndef DECODE_GZ_SILENCE
        printf(
            "%s\n",
            "please run init_decode_gz() and pass malloc before running "
            "decode_gz. Exiting...");
        #endif
        return NULL;
    }
    DecodedData * return_value =
        (DecodedData *)malloc_func(sizeof(DecodedData));
    
    if (compressed_bytes == NULL) {
        return_value->good = false;
        return return_value;
    }
    
    // The first chunk must be IHDR
    if (compressed_bytes_left < sizeof(GZHeader)) {
        #ifndef DECODE_GZ_SILENCE
        printf("data stream too tiny to even contain a header\n");
        #endif
        return_value->good = false;
        return return_value;
    }
    
    GZHeader * gzip_header = consume_struct(
        /* type  : */ GZHeader,
        /* buffer: */ &compressed_bytes,
        /* buffer_size: */ &compressed_bytes_left);
    
    #ifndef DECODE_GZ_SILENCE 
    printf("checking if valid gzip file..\n");
    #endif
    
    if (gzip_header->id1 != 31
        || gzip_header->id2 != 139)
    {
        #ifndef DECODE_GZ_SILENCE
        printf("not a valid gzip file - incorrect header\n");
        #endif
        return_value->good = false;
        return return_value;
    }
    
    if (gzip_header->CM != 8) {
        #ifndef DECODE_GZ_SILENCE
        printf("unsupported gzip - no DEFLATE compression~\n");
        #endif
        return_value->good = false;
        return return_value;
    }
    
    /*
    FLG (FLaGs)
       This flag byte is divided into bits as follows:
       bit 0   FTEXT
       bit 1   FHCRC
       bit 2   FEXTRA
       bit 3   FNAME
       bit 4   FCOMMENT
       bit 5   reserved
       bit 6   reserved
       bit 7   reserved
    */
    #ifndef DECODE_GZ_SILENCE
    printf(
        "gzip_header->FLG[FTEXT]: %u\n",
        gzip_header->FLG >> 0 & 1);
    printf(
        "gzip_header->FLG[FHCRC]: %u\n",
        gzip_header->FLG >> 1 & 1);
    printf(
        "gzip_header->FLG[FEXTRA]: %u\n",
        gzip_header->FLG >> 2 & 1);
    printf(
        "gzip_header->FLG[FNAME]: %u\n",
        gzip_header->FLG >> 3 & 1);
    printf(
        "gzip_header->FLG[FCOMMENT]: %u\n",
        gzip_header->FLG >> 4 & 1);
    #endif
    
    /*    
    (if FLG.FNAME set)
    
    +=========================================+
    |...original file name, zero-terminated...|
    +=========================================+
    */
    if (gzip_header->FLG >> 3 & 1) {
        #ifndef DECODE_GZ_SILENCE
        printf("original filename was included, reading...\n");
        #endif
        
        char * filename = consume_till_terminate(
            /* uint8_t * from,: */ &compressed_bytes,
            /* uint32_t * from_size: */ &compressed_bytes_left,
            /* uint32_t max_size: */ compressed_bytes_left,
            /* const char terminator: */ '\0');
        
        #ifndef DECODE_GZ_SILENCE
        
        printf(
            "original filename: %s\n",
            filename);
        #endif
    }
    
    /*
    if FLG.FCOMMENT set)
    
    +===================================+
    |...file comment, zero-terminated...| (more-->)
    +===================================+ 
    */
    if (gzip_header->FLG >> 4 & 1) {
        #ifndef DECODE_GZ_SILENCE
        char * comment = consume_till_terminate(
            /* uint8_t ** from: */ &compressed_bytes,
            /* from_size: */ &compressed_bytes_left,
            /* max_size: */ compressed_bytes_left,
            /* terminator: */ (char)0);
        
        printf("comment: %s\n", comment);
        #endif
    }


    #ifndef DECODE_GZ_SILENCE
    printf("compressed blocks should start here...\n");
    printf("file size left: %u\n", compressed_bytes_left);
    printf(
        "8 bytes at EOF rsrved, so DEFLATE size is: %u\n",
        compressed_bytes_left - 8);
    #endif
    
    // TODO: figure out how much memory to assign in advance
    uint32_t guess_decompressed_size = (compressed_bytes_left - 8) * 25;
    
    uint8_t * recipient = (uint8_t *)malloc_func(guess_decompressed_size);
    uint64_t recipient_size = 0;
    
    uint64_t temp_working_memory_size = 50000000;
    uint8_t * temp_working_memory = (uint8_t *)malloc_func(
        temp_working_memory_size);
    
    uint32_t inflate_good = false;
    
    inflate(
        /* uint8_t const * recipient: */
            recipient,
        /* const uint64_t recipient_size: */
            guess_decompressed_size,
        /* uint64_t * final_recipient_size: */
            &recipient_size,
        /* uint8_t const * temp_working_memory: */
            temp_working_memory,
        /* const uint64_t temp_working_memory_size: */
            temp_working_memory_size,
        /* uint8_t const * compressed_input: */
            compressed_bytes,
        /* const uint64_t compressed_input_size: */
            compressed_bytes_left - 8,
        /* uint32_t * out_good: */
            &inflate_good);
   
    #ifndef DECODE_GZ_SILENCE 
    printf("\ninflate algorithm returned: %u\n", inflate_good);
    #endif
    if (!inflate_good) {
        return return_value;
    }
    
    GZFooter * gzip_footer = consume_struct(
        /* type: */ GZFooter,
        /* buffer: */ &compressed_bytes,
        /* ptr_buffer_size: */ &compressed_bytes_left);
    
    #ifndef DECODE_GZ_SILENCE 
    printf("CRC32 value from footer: %u\n", gzip_footer->CRC32);
    printf("ISIZE value from footer: %u\n", gzip_footer->ISIZE);
    
    printf(
        "size left in buffer (excess bytes): %u\n",
        compressed_bytes_left);
    
    printf("end of gz file...\n");
    #endif
    
    return_value->data = (char *)recipient;
    return_value->good = true;
    
    return return_value;
}

#undef true
#undef false
