#include "decode_gz.h"

#define true 1
#define false 0

#ifndef DECODE_GZ_SILENCE
static char * consume_till_terminate(
    DataStream * from,
    const uint32_t max_size,
    const char terminator)
{
    #ifndef DECODE_GZ_IGNORE_ASSERTS
    assert(max_size <= from->size_left);
    #endif
    
    uint32_t string_size = 0;
    char * seeker = (char *)from->data;
    while (
        *seeker != terminator 
        && string_size < max_size)
    {
        string_size++;
        seeker++;
    }
    
    #ifndef DECODE_GZ_IGNORE_ASSERTS
    assert(string_size > 0);
    #endif
    
    char * return_value = (char *)malloc(
        string_size * sizeof(char));
    
    for (uint32_t i = 0; i <= string_size; i++) {
        #ifndef DECODE_GZ_IGNORE_ASSERTS
        assert(from->size_left > 0);
        #endif
        return_value[i] = ((char *)from->data)[0];
        from->data++;
        from->size_left--;
    }
    
    return return_value;
}
#endif

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
    uint32_t compressed_bytes_size)
{
    DecodedData * return_value =
        (DecodedData *)malloc(sizeof(DecodedData));
    
    if (compressed_bytes == NULL) {
        return_value->good = false;
        return return_value;
    }
    
    DataStream * data_stream =
        (DataStream *)malloc(sizeof(DataStream));
    
    data_stream->data = compressed_bytes;
    data_stream->size_left = compressed_bytes_size;
    
    // The first chunk must be IHDR
    if (data_stream->size_left < sizeof(GZHeader)) {
        #ifndef DECODE_GZ_SILENCE
        printf("data stream too tiny to even contain a header\n");
        #endif
        return_value->good = false;
        return return_value;
    }
    
    GZHeader * gzip_header = consume_struct(
        /* type  : */ GZHeader,
        /* buffer: */ data_stream);
   
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
    |...original file name, zero-terminated...| (more-->)
    +=========================================+
    */
    if (gzip_header->FLG >> 3 & 1) {
        #ifndef DECODE_GZ_SILENCE
        printf("original filename was included, reading...\n");
        #endif

        #ifndef DECODE_GZ_SILENCE
        char * filename = consume_till_terminate(
            /* from: */ data_stream,
            /* max_size: */ data_stream->size_left,
            /* terminator: */ (char)0);
        
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
            /* from: */ data_stream,
            /* max_size: */ data_stream->size_left,
            /* terminator: */ (char)0);
        
        printf("comment: %s\n", comment);
        #endif
    }


    #ifndef DECODE_GZ_SILENCE
    printf("compressed blocks should start here...\n");
    printf("file size left: %zu\n", data_stream->size_left);
    printf(
        "8 bytes at EOF rsrved, so DEFLATE size is: %zu\n",
        data_stream->size_left - 8);
    #endif
    
    // TODO: figure out how much memory to assign in advance
    uint32_t decompressed_size = data_stream->size_left * 25;
    
    uint8_t * recipient = (uint8_t *)malloc(decompressed_size);
    
    inflate(
        /* recipient: */ recipient,
        /* recipient_size: */ decompressed_size,
        /* data_stream: */ data_stream,
        /* compressed_size_bytes: */ data_stream->size_left - 8);
   
    #ifndef DECODE_GZ_SILENCE 
    printf("\ninflate algorithm decompressed & returned\n");
    #endif
    
    GZFooter * gzip_footer = consume_struct(
        /* type: */ GZFooter,
        /* buffer: */ data_stream);
    
    #ifndef DECODE_GZ_SILENCE 
    printf("CRC32 value from footer: %u\n", gzip_footer->CRC32);
    printf("ISIZE value from footer: %u\n", gzip_footer->ISIZE);
    
    printf(
        "size left in buffer (excess bytes): %u\n",
        (int)data_stream->size_left);
    
    printf("end of gz file...\n");
    #endif
    
    free(gzip_header);
    free(gzip_footer);
    
    return_value->data = (char *)recipient;
    return_value->good = true;
    
    return return_value;
}

#undef true
#undef false
