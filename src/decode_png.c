#include "decode_png.h"

#define DECODE_PNG_SILENCE
// #define DECODE_PNG_IGNORE_CRC_CHECKS
#define DECODE_PNG_IGNORE_ASSERTS

#ifndef DECODE_PNG_SILENCE
#include "stdio.h"
#endif

#ifndef DECODE_PNG_IGNORE_ASSERTS
#include "assert.h"
#endif

/*
PNG files include CRC 'cyclic redundancy checks', a kind
of checksum to make sure each block is valid data.
*/

#ifndef DECODE_PNG_IGNORE_CRC_CHECKS
/*
Table of CRCs of all 8-bit messages.

The table itself is not found in the spec, but the spec
has a snippet of C code that generates it
*/
static unsigned long crc_table[256] = {
    0,
    1996959894,
    3993919788,
    2567524794,
    124634137,
    1886057615,
    3915621685,
    2657392035,
    249268274,
    2044508324,
    3772115230,
    2547177864,
    162941995,
    2125561021,
    3887607047,
    2428444049,
    498536548,
    1789927666,
    4089016648,
    2227061214,
    450548861,
    1843258603,
    4107580753,
    2211677639,
    325883990,
    1684777152,
    4251122042,
    2321926636,
    335633487,
    1661365465,
    4195302755,
    2366115317,
    997073096,
    1281953886,
    3579855332,
    2724688242,
    1006888145,
    1258607687,
    3524101629,
    2768942443,
    901097722,
    1119000684,
    3686517206,
    2898065728,
    853044451,
    1172266101,
    3705015759,
    2882616665,
    651767980,
    1373503546,
    3369554304,
    3218104598,
    565507253,
    1454621731,
    3485111705,
    3099436303,
    671266974,
    1594198024,
    3322730930,
    2970347812,
    795835527,
    1483230225,
    3244367275,
    3060149565,
    1994146192,
    31158534,
    2563907772,
    4023717930,
    1907459465,
    112637215,
    2680153253,
    3904427059,
    2013776290,
    251722036,
    2517215374,
    3775830040,
    2137656763,
    141376813,
    2439277719,
    3865271297,
    1802195444,
    476864866,
    2238001368,
    4066508878,
    1812370925,
    453092731,
    2181625025,
    4111451223,
    1706088902,
    314042704,
    2344532202,
    4240017532,
    1658658271,
    366619977,
    2362670323,
    4224994405,
    1303535960,
    984961486,
    2747007092,
    3569037538,
    1256170817,
    1037604311,
    2765210733,
    3554079995,
    1131014506,
    879679996,
    2909243462,
    3663771856,
    1141124467,
    855842277,
    2852801631,
    3708648649,
    1342533948,
    654459306,
    3188396048,
    3373015174,
    1466479909,
    544179635,
    3110523913,
    3462522015,
    1591671054,
    702138776,
    2966460450,
    3352799412,
    1504918807,
    783551873,
    3082640443,
    3233442989,
    3988292384,
    2596254646,
    62317068,
    1957810842,
    3939845945,
    2647816111,
    81470997,
    1943803523,
    3814918930,
    2489596804,
    225274430,
    2053790376,
    3826175755,
    2466906013,
    167816743,
    2097651377,
    4027552580,
    2265490386,
    503444072,
    1762050814,
    4150417245,
    2154129355,
    426522225,
    1852507879,
    4275313526,
    2312317920,
    282753626,
    1742555852,
    4189708143,
    2394877945,
    397917763,
    1622183637,
    3604390888,
    2714866558,
    953729732,
    1340076626,
    3518719985,
    2797360999,
    1068828381,
    1219638859,
    3624741850,
    2936675148,
    906185462,
    1090812512,
    3747672003,
    2825379669,
    829329135,
    1181335161,
    3412177804,
    3160834842,
    628085408,
    1382605366,
    3423369109,
    3138078467,
    570562233,
    1426400815,
    3317316542,
    2998733608,
    733239954,
    1555261956,
    3268935591,
    3050360625,
    752459403,
    1541320221,
    2607071920,
    3965973030,
    1969922972,
    40735498,
    2617837225,
    3943577151,
    1913087877,
    83908371,
    2512341634,
    3803740692,
    2075208622,
    213261112,
    2463272603,
    3855990285,
    2094854071,
    198958881,
    2262029012,
    4057260610,
    1759359992,
    534414190,
    2176718541,
    4139329115,
    1873836001,
    414664567,
    2282248934,
    4279200368,
    1711684554,
    285281116,
    2405801727,
    4167216745,
    1634467795,
    376229701,
    2685067896,
    3608007406,
    1308918612,
    956543938,
    2808555105,
    3495958263,
    1231636301,
    1047427035,
    2932959818,
    3654703836,
    1088359270,
    936918000,
    2847714899,
    3736837829,
    1202900863,
    817233897,
    3183342108,
    3401237130,
    1404277552,
    615818150,
    3134207493,
    3453421203,
    1423857449,
    601450431,
    3009837614,
    3294710456,
    1567103746,
    711928724,
    3020668471,
    3272380065,
    1510334235,
    755167117
};

/*
Update a running CRC with the bytes buf[0..len-1]--the CRC
should be initialized to all 1's, and the transmitted value
is the 1's complement of the final running CRC (see the
crc() routine below).
*/
static unsigned long update_crc(
    const unsigned long crc,
    const unsigned char *buf,
    const uint32_t len)
{
    assert(buf != NULL);
    // assert(len > 0);
    
    unsigned long c = crc;
    uint32_t n;
    
    for (n = 0; n < len; n++) {
        #ifndef DECODE_PNG_IGNORE_ASSERTS
        assert(((c ^ buf[n]) & 0xff) < 256);
        #endif
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    
    return c;
}
#endif // DECODE_PNG_IGNORE_CRC_CHECKS

/*
A PNG file starts with an 8-byte signature
All of these seem like ancient artifacts to me,
except for bytes 2 to 4, which read 'PNG'. Actually
the other values seem to always be the same in every PNG
also, so you can just use the whole shebang as a check
to find out if a given file is a PNG.
*/
#pragma pack(push, 1)
typedef struct PNGSignature {
    uint8_t highbit_hint;             // artifact
    char png_string[3];               // Reads 'PNG'
    char dos_style_line_ending[2];    // DOS artifact
    char end_of_file_char;            // DOS artifact
    char unix_style_line_ending;      // Unix/DOS artifact
} PNGSignature;

/*
It's then divided into 'chunks' of data, each of which starts
with this header.
*/
typedef struct PNGChunkHeader {
    uint32_t length;     // big endian
    char type[4];        // 1st char uppercase = critical chunk
} PNGChunkHeader;

/*
IHDR must be the first chunk type
it contains this data (13 bytes)
*/
typedef struct IHDRBody {
    uint32_t width;
    uint32_t height;
    uint8_t  bit_depth;
    uint8_t  color_type;
    uint8_t  compression_method;
    uint8_t  filter_method;
    uint8_t  interlace_method;
} IHDRBody;

/*
chunks with type IDAT are crucial
they contain the compressed image data

the only compression method supported is 'zlib' aka 'DEFLATE',
that's why we need our header file inflate.h to read it
*/
typedef struct ZLIBHeader {
    uint8_t zlibmethodflags;
    uint8_t additionalflags;
} ZLIBHeader;


typedef struct PNGFooter {
    uint32_t CRC;
} PNGFooter;

typedef struct ZLIBFooter {
    uint32_t adler_32_checksum;
} ZLIBFooter;
#pragma pack(pop)

/*
PNG files have many 4-byte big endian values
We need to able to flip them to little endian
*/
static uint32_t flip_endian(const uint32_t to_flip) {
    
    uint32_t byte_1 = ((to_flip >> 0) & 255);
    uint32_t byte_2 = ((to_flip >> 8) & 255);
    uint32_t byte_3 = ((to_flip >> 16) & 255);
    uint32_t byte_4 = ((to_flip >> 24) & 255);
    
    uint32_t return_value =
        byte_1 << 24 |
        byte_2 << 16 |
        byte_3 << 8  |
        byte_4 << 0;
    
    return return_value;
}

static uint32_t are_equal_strings(
    char * str1,
    char * str2,
    size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (str1[i] != str2[i]) {
            return 0;
        }
    }
    
    return 1;
}

/*
One of the reconstruction algorithms (see undo_PNG_filter below)
is a 'paeth predictor'

The PNG specification has sample code for the paeth predictor,
so I just copy pasted it here below.
*/
static uint8_t compute_paeth_predictor(
    /* a_previous_pixel */ int32_t a,
    /* b_previous_scanline */ int32_t b,
    /* c_previous_scanline_previous_pixel: */ int32_t c)
{
    /*
    sample Code from the specification:
        p = a + b - c
        pa = abs(p - a)
        pb = abs(p - b)
        pc = abs(p - c)
        if pa <= pb and pa <= pc then Pr = a
        else if pb <= pc then Pr = b
        else Pr = c
        return Pr
    
    'The calculations within the PaethPredictor function
    shall be performed exactly, without overflow.'
    */
    
    int32_t Pr_paeth = 0;
    
    int32_t p = a + b - c;
    
    int32_t pa = p - a;
    if (pa < 0) { pa *= -1; }
    
    int32_t pb = p - b;
    if (pb < 0) { pb *= -1; }
    
    int32_t pc = p - c;
    if (pc < 0) { pc *= -1; }
    
    if (pa <= pb && pa <= pc) {
        // code path not hit in working PNG
    Pr_paeth = a;
    } else if (pb <= pc) {
        // code path not hit in working PNG
    Pr_paeth = b;
    } else {
        // code path not hit in working PNG
    Pr_paeth = c;
    }
    
    return (uint8_t)Pr_paeth;
}

/*
PNG files have to be 'reconstructed' even after all of the 
decompression is finished. The original RGBA values are not
stored - the 'transformed' or 'filtered' values are stored
instead, and to undo these transforms you need several values
from other pixels. You can read about this in the specification
but it might take some struggling to understand.
*/
static uint8_t undo_PNG_filter(
    unsigned int filter_type,
    uint8_t original_value,
    uint8_t a_previous_pixel,
    uint8_t b_previous_scanline,
    uint8_t c_previous_scanline_previous_pixel)
{
    if (filter_type == 0) {
    return original_value;
    } else if (filter_type == 1) {
    return original_value + a_previous_pixel;
    } else if (filter_type == 2) {
    return original_value + b_previous_scanline;
    } else if (filter_type == 3) {
        uint32_t avg =
            ((uint32_t)a_previous_pixel +
            (uint32_t)b_previous_scanline) / 2;
    return original_value + (uint8_t)avg;
    } else if (filter_type == 4) {
        uint8_t computed_paeth =
            compute_paeth_predictor(
        /* a_previous_pixel   : */
                    (int32_t)a_previous_pixel,
        /* b_previous_scanline: */
                    (int32_t)b_previous_scanline,
                /* c_prev_scl_prev_pxl: */
                    (int32_t)c_previous_scanline_previous_pixel);
        uint8_t return_value = original_value + computed_paeth;
        
    return return_value; 
    } else {
        #ifndef DECODE_PNG_SILENCE
        printf(
            "ERROR - unsupported filter type: %u. Expect 1-4\n",
            filter_type);
        #endif 
        
        #ifndef DECODE_PNG_IGNORE_ASSERTS
        assert(0);
        #endif
    }
    
    return 0;
}

void get_PNG_width_height(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good)
{
    if (compressed_input_size < 26) {
        #ifndef DECODE_PNG_SILENCE
        printf("ERROR - need 26 bytes for dimension check\n");
        #endif
        #ifndef DECODE_PNG_IGNORE_ASSERTS
        assert(0);
        #endif
        *out_width = 0;
        *out_height = 0;
        *out_good = 0;
        return;
    }
    
    // 5 bytes
    PNGSignature png_signature = *(PNGSignature *)compressed_input;
    compressed_input += sizeof(PNGSignature);
    
    if (!are_equal_strings(
        /* string 1: */ png_signature.png_string,
        /* string 2: */ (char *)"PNG",
        /* string length: */ 3))
    {
        #ifndef DECODE_PNG_SILENCE
        printf("aborting - not a PNG file\n");
        #endif
        *out_width = 0;
        *out_height = 0;
        *out_good = 0;
        return;
    } else {
        #ifndef DECODE_PNG_SILENCE
        printf("found PNG header\n");
        #endif
    }
    
    #ifndef DECODE_PNG_IGNORE_ASSERTS
    assert(entire_file.bits_left == 0);
    #endif
    
    // 8 bytes
    PNGChunkHeader chunk_header = *(PNGChunkHeader *)compressed_input;
    compressed_input += sizeof(PNGChunkHeader);
    
    assert(
        are_equal_strings(
            chunk_header.type,
            (char *)"IHDR",
            4));
    
    // 13 bytes
    IHDRBody ihdr_body = *(IHDRBody *)compressed_input;
    compressed_input += sizeof(IHDRBody);
    
    *out_width = flip_endian(ihdr_body.width);
    *out_height = flip_endian(ihdr_body.height);
    #ifndef DECODE_PNG_SILENCE
    printf(
        "succes! width/height: [%u,%u]\n",
        *out_width,
        *out_height);
    #endif
    
    *out_good = 1;
}

void decode_PNG(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint8_t * out_rgba_values,
    uint64_t rgba_values_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good)
{
    /*
        We start out with compressed_input (allocated)
        We have out_rgba_values_size allocated in
        out_rgba_values.
    */
    *out_good = 0;
    
    #ifndef DECODE_PNG_IGNORE_ASSERTS
    assert(compressed_input_size > 0);
    #endif
    
    uint64_t compressed_input_size_left =
        compressed_input_size;
    
    PNGSignature png_signature =
        *(PNGSignature *)compressed_input;
    compressed_input += sizeof(PNGSignature);
    
    #ifndef DECODE_PNG_SILENCE
    printf(
        "Signature's png_string (expecting 'PNG'): %c%c%c\n",
        png_signature.png_string[0],
        png_signature.png_string[1],
        png_signature.png_string[2]);
    #endif
    
    if (
        !are_equal_strings(
            /* string 1: */ png_signature.png_string,
            /* string 2: */ (char *)"PNG",
            /* string length: */ 3))
    {
        #ifndef DECODE_PNG_SILENCE
        printf("aborting - not a PNG file\n");
        #endif
        *out_good = 0;
        return;
    }
    
    // these pointers are initted below
    IHDRBody ihdr_body;
    uint8_t * headerless_compressed_data = NULL;
    uint8_t * headerless_compressed_data_begin = NULL;
    uint32_t headerless_compressed_data_stream_size = 0;
    uint8_t * decoded_stream = NULL;
    uint8_t * decoded_stream_start = NULL;
    unsigned int decompressed_size = 0;
    
    uint32_t found_first_IDAT = 0;
    uint32_t ran_inflate_algorithm = 0;
    uint32_t found_IHDR = 0;
    uint32_t found_IEND = 0;
    
    while (
        compressed_input_size_left >= 8
        && !found_IEND)
    {
        #ifndef DECODE_PNG_SILENCE
        printf(
            "%u bytes left in file, read another PNG chunk..\n",
            compressed_input_size_left);
        #endif
        
        #ifndef DECODE_PNG_IGNORE_CRC_CHECKS
        unsigned long running_crc = 0xffffffffL;
        #endif
        
        PNGChunkHeader chunk_header = *(PNGChunkHeader *)compressed_input;
        compressed_input += sizeof(PNGChunkHeader);
        
        chunk_header.length = flip_endian(chunk_header.length);
        
        if (
            !are_equal_strings(
                chunk_header.type,
                (char *)"IDAT",
                4)
            && found_first_IDAT)
        {
            #ifndef DECODE_PNG_SILENCE
            printf("next chunk is not IDAT, ");
            printf("so all compressed data was collected.\n");
            #endif
            
            assert(decoded_stream != NULL);
            assert(headerless_compressed_data_stream_size > 4);
            ran_inflate_algorithm = 1;
            uint32_t inflate_result = 0;
            inflate(
                /* recipient: */
                    decoded_stream,
                /* recipient_size: */
                    decompressed_size,
                /* compressed_input: */
                    headerless_compressed_data_begin,
                /* compressed_input_size: */
                    headerless_compressed_data_stream_size - 4,
                /* good: */
                    &inflate_result);
            
            if (inflate_result == 0) {
                #ifndef DECODE_PNG_SILENCE
                printf("INFLATE algorithm failed\n");
                #endif
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            } else {
                #ifndef DECODE_PNG_SILENCE
                printf("INFLATE succesful\n");
                #endif
            }
        }
        
        #ifndef DECODE_PNG_IGNORE_CRC_CHECKS
        running_crc = update_crc(
            /* crc: */ running_crc,
            /* buffer: */ (unsigned char *)&(chunk_header.type),
            /* length: */ 4);
        running_crc = update_crc(
            /* crc: */ running_crc,
            /* buffer: */ (unsigned char *)compressed_input,
            /* length: */ chunk_header.length);
        running_crc = running_crc ^ 0xffffffffL;
        #endif
        
        #ifndef DECODE_PNG_SILENCE 
        printf(
            "[%s] chunk (%u bytes)\n",
            chunk_header.type,
            chunk_header.length);
        #endif
        
        if (chunk_header.length >= compressed_input_size_left) {
            
            #ifndef DECODE_PNG_SILENCE 
            printf(
            "failing to decode PNG, [%s] chunk of length %u is larger than remaining file size of %u\n",
            chunk_header.type,
            chunk_header.length,
            compressed_input_size_left);
            #endif
            if (headerless_compressed_data) { free(headerless_compressed_data_begin); }
            if (decoded_stream) { free(decoded_stream_start); }
            *out_good = 0;
            return;
        }
        
        if (are_equal_strings(
            chunk_header.type,
            (char *)"PLTE",
            4))
        {
            if (decoded_stream == NULL)
            {
                #ifndef DECODE_PNG_SILENCE 
                printf(
                    "failing to decode PNG. [%s] chunk should never appear before [IHDR] chunk\n",
                    chunk_header.type);
                #endif
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            }
        } else if (are_equal_strings(
            chunk_header.type,
            (char *)"IHDR",
            4))
        {
            found_IHDR = 1;
            #ifndef DECODE_PNG_IGNORE_ASSERTS
            assert(entire_file.bits_left == 0);
            #endif
            ihdr_body = *(IHDRBody *)compressed_input;
            compressed_input += sizeof(IHDRBody);
            #ifndef DECODE_PNG_IGNORE_ASSERTS
            assert(entire_file.bits_left < 8);
            #endif
            *out_width =
                flip_endian(ihdr_body.width);
            *out_height =
                flip_endian(ihdr_body.height);
            
            decompressed_size =
                (*out_width)
                    * (*out_height)
                    * 5;
            
            // (below) These PNG setting assert the RGBA
            // color space.
            // There are many other formats (greyscale
            // illustrations etc.) found in .png files that we
            // are not supporting for now. 
            if (ihdr_body.bit_depth != 8)
            {
                #ifndef DECODE_PNG_SILENCE
                printf(
                    "unsupported PNG bit depth %u\n",
                    ihdr_body.bit_depth);
                #endif
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            }
            
            // color type 2: each pixel is RGB
            // color type 3: indexed color? (unsupported)
            // color type 6: each pixel is RGBA
            if (ihdr_body.color_type != 6
                && ihdr_body.color_type != 2)
            {
                #ifndef DECODE_PNG_SILENCE
                printf(
                    "unsupported PNG color type: %u (expected 2 or 6)\n",
                    ihdr_body.color_type);
                #endif
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            }
            
            #ifndef DECODE_PNG_SILENCE
            if (ihdr_body.color_type != 6) {
                printf("error - only color type 6 is supported");
            } else {
                printf("\t\t(Truecolor with alpha)\n");
            }
            
            printf(
                "\tcompression_method: %u\n",
                ihdr_body.compression_method);
            printf(
                "\tfilter_method: %u\n",
                ihdr_body.filter_method);
            printf(
                "\tinterlace_method: %u\n",
                ihdr_body.interlace_method);
            #endif
            
            if (ihdr_body.filter_method != 0) {
                #ifndef DECODE_PNG_SILENCE 
                printf(
                    "failing to decode PNG. filter method in [IHDR] chunk must be 0\n");
                #endif
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            }
            
            if (compressed_input_size_left < 4)
            {
                #ifndef DECODE_PNG_SILENCE 
                printf(
                    "failing to decode PNG - file size left is %u bytes\n",
                    compressed_input_size_left);
                #endif
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                *out_good = 0;
                return;
            }
            
            #ifndef DECODE_PNG_IGNORE_ASSERTS
            assert(decoded_stream == NULL);
            #endif
            
            decoded_stream =
                (uint8_t *)malloc(decompressed_size);
            decoded_stream_start = decoded_stream;
            
            // this copy (headerless_compressed_data) is necessary because
            // the data needed for DEFLATE is likely spread
            // across multiple chunks with useless header data
            // & checksums sprinkled in between
            // we'll concatenate all the data into headerless_compressed_data
            // first, then INFLATE afterwards
            headerless_compressed_data =
                (uint8_t *)malloc(decompressed_size);
            headerless_compressed_data_begin = headerless_compressed_data;
        }  else if (are_equal_strings(
            chunk_header.type,
            (char *)"IDAT",
            4))
        {
            if (!found_IHDR
                || decoded_stream == NULL
                || headerless_compressed_data == NULL)
            {
                #ifndef DECODE_PNG_SILENCE
                printf("failing to decode PNG - no [IHDR] chunk was found, yet already in [IDAT] chunk\n");
                #endif
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            }
            
            uint32_t chunk_data_length =
                chunk_header.length;
            
            if (!found_first_IDAT) {
                #ifndef DECODE_PNG_SILENCE
                printf("\t1st IDAT chunk, read zlib headers..\n");
                #endif
                found_first_IDAT = 1;
                
                ZLIBHeader zlib_header = *(ZLIBHeader *)compressed_input;
                compressed_input += sizeof(ZLIBHeader);
                chunk_data_length -= sizeof(ZLIBHeader);
                
                // to mask the rightmost 4 bits we need 00001111 
                //                                          8421
                //                                    8+4+2+1=15
                // to isolate the leftmost 4, we need to
                // shift right by 4
                // (I guess the new padding bits on the left
                // are 0 by default)
                uint8_t compression_method =
                    zlib_header.zlibmethodflags & 15;
                #ifndef DECODE_PNG_SILENCE
                uint8_t compression_info =
                    zlib_header.zlibmethodflags >> 4;
                printf(
                    "\t\tcompression method: %u\n",
                    compression_method);
                printf(
                    "\t\tcompression info: %u\n",
                    compression_info);
                #endif
                if (compression_method != 8) {
                    *out_good = 0;
                    if (decoded_stream) { free(decoded_stream_start); }
                    if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                    return;
                }
                
                /* 
                first FIVE bits is FCHECK
                the 'check bits for CMF and FLG'
                to mask the rightmost 5 bits: 15 + 16 = 31
                spec:
                "The FCHECK value must be such that CMF and FLG,
                when viewed as a 16-bit unsigned integer stored in
                MSB order (CMF*256 + FLG), is a multiple of 31."
                */
                uint32_t full_check_value =
                    (uint16_t)
                        (zlib_header.additionalflags
                    |
                    (uint16_t)
                        (zlib_header.zlibmethodflags << 8));
                #ifndef DECODE_PNG_SILENCE
                printf(
                    "\t\tfull 16-bit check val (use FCHECK): %u - ",
                    full_check_value);
                printf(
                    full_check_value % 31 == 0 ? "OK\n" : "ERR\n");
                #endif
                
                if (full_check_value == 0
                    || full_check_value % 31 != 0)
                {
                    *out_good = 0;
                    if (decoded_stream) { free(decoded_stream_start); }
                    if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                    return;
                }
                
                /* 
                sixth bit is FDICT, the 'preset dictionary' flag
                If FDICT is set, a DICT dictionary identifier is
                present immediately after the FLG byte. The
                dictionary is a sequence of bytes which are
                initially fed to the compressor without producing
                any compressed output. DICT is the Adler-32
                checksum of this sequence of bytes (see the
                definition of ADLER32 below). 
                
                The decompressor can use this identifier to determine
                which dictionary has been used by the compressor.
                */
                uint8_t FDICT =
                    zlib_header.additionalflags >> 5 & 1;
                
                if (FDICT) {
                    #ifndef DECODE_PNG_SILENCE
                    printf("\t\tdiscarding FDICT... (4 bytes)\n");
                    #endif
                    compressed_input += 4;
                    compressed_input_size_left -= 4;
                    chunk_data_length -= 4;
                }
                
                /*
                7th & 8th bit is FLEVEL
                FLEVEL (Compression level)
                The "dflate" method (CM = 8) sets these flags as
                follows:
                
                0 - compressor used fastest algorithm
                1 - compressor used fast algorithm
                2 - compressor used default algorithm
                3 - compressor used max compression, slowest algo
                
                The information in FLEVEL is not needed for
                decompression; it is there to indicate if
                recompression might be worthwhile.
                */
                if (FDICT != 0) {
                    *out_good = 0;
                    if (decoded_stream) { free(decoded_stream_start); }
                    if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                    return;
                }
                
                #ifndef DECODE_PNG_SILENCE
                uint8_t FLEVEL =
                    zlib_header.additionalflags >> 6 & 3;
                printf("\t\tFDICT: %u\n", FDICT);
                printf("\t\tFLEVEL: %u\n", FLEVEL);
                printf("\t\tread compressed data...\n");
                printf(
                    "\t\tprepped recipient of %u bytes...\n",
                    decompressed_size);
                #endif
                
            }
            
            #ifndef DECODE_PNG_SILENCE
            printf(
                "\t\tcopying %u bytes to compressed stream...\n",
                chunk_data_length);
            #endif
            
            for (uint32_t _ = 0; _ < chunk_data_length; _++)
            {
                *headerless_compressed_data++ =
                    *(uint8_t *)compressed_input++;
                headerless_compressed_data_stream_size++;
                compressed_input_size_left--;
            }
        }
        else if (are_equal_strings(
            chunk_header.type,
            (char *)"IEND",
            4))
        {
            if (decoded_stream == NULL) {
                *out_good = 0;
                if (decoded_stream) { free(decoded_stream_start); }
                if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
                return;
            }
            
            // handle image end header
            #ifndef DECODE_PNG_SILENCE
            printf("found IEND header\n");
            #endif
            found_IEND = 1;
        }
        else if ((char)chunk_header.type[0] > 'Z')
        {
            #ifndef DECODE_PNG_SILENCE
            printf(
                "\tskipping unhandled non-critical chunk...\n");
            #endif
            compressed_input += chunk_header.length;
            compressed_input_size_left -= chunk_header.length;
        } else {
            #ifndef DECODE_PNG_SILENCE
            printf(
                "ERROR: unhandled critical chunk header: %s\n",
                chunk_header.type);
            #endif
            if (decoded_stream) { free(decoded_stream_start); }
            if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
            *out_good = 0;
            return;
        }
        
        if (compressed_input_size_left < 4)
        {
            #ifndef DECODE_PNG_SILENCE
            printf(
                "failed to decode PNG - unexpected remaining file size of %u\n",
                compressed_input_size_left);
            #endif
            *out_good = 0;
            if (decoded_stream) { free(decoded_stream_start); }
            if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
            return;
        }
        
        PNGFooter block_footer = *(PNGFooter *)compressed_input;
        compressed_input += sizeof(PNGFooter);
        
        #ifndef DECODE_PNG_IGNORE_CRC_CHECKS
        block_footer.CRC = flip_endian(block_footer.CRC);
        if (running_crc != block_footer.CRC) {
            #ifndef DECODE_PNG_SILENCE
            printf("ERROR: CRC checksum mismatch - this PNG file is corrupted.\n");
            #endif
            if (decoded_stream) { free(decoded_stream_start); }
            if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
            *out_good = 0;
            return;
        } else {
            #ifndef DECODE_PNG_SILENCE
            printf("CRC checksum match: OK\n");
            #endif
        }
        #endif
    }
    // end of "while size file > pngchunkheader" loop
    
    if (!ran_inflate_algorithm) {
        #ifndef DECODE_PNG_SILENCE
        printf("Failed to identify the last iDAT chunk, didn't run inflate algorithm\n");
        #endif
        
        *out_good = 0;
        if (decoded_stream) { free(decoded_stream_start); }
        if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
        return;
    }
    
    // we've uncompressed the data using DEFLATE
    //
    // now let's undo the filtering methods
    // we already asserted the IHDR filter method was 0,
    // filter_type is something else and the first num
    // in the uncompressed datastream
    // "filters" could have been called "transforms",
    // not sure how it's a filter
    
    #ifndef DECODE_PNG_SILENCE
    printf(
        "\n\nreconstructing (un-doing PNG filters)...\n");
    #endif
    
    uint32_t pixel_count =
        (*out_width) * (*out_height);
    
    if (*out_width < 1 || *out_height < 1) {
        #ifndef DECODE_PNG_SILENCE
        printf(
            "ERROR - image width/height was [%u,%u]\n",
            *out_width,
            *out_height);
        #endif
        *out_good = 0;
        if (decoded_stream) { free(decoded_stream_start); }
        if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
        return;
    }
    
    decoded_stream = decoded_stream_start;
    uint8_t * rgba_at = out_rgba_values;
    
    /*
    The spec tells us to track these values:
    
    x = the byte being filtered;
    a = the byte in the pixel immediately
        before the pixel containing x
    b = the byte in the previous scanline
    c = the byte in the pixel immediately
        before the pixel containing b 
    
    [c][b]
    [a][x]
    */
    uint8_t bytes_per_channel =
        ihdr_body.color_type == 2 ? 3 : 4;
    #ifndef DECODE_PNG_SILENCE
    printf(
        "bytes per channel: %u because ihdr_body.color_type was: %u\n",
        bytes_per_channel,
        ihdr_body.color_type);
    #endif
    uint8_t * a_previous_pixel =
        out_rgba_values - bytes_per_channel;
    uint8_t * b_previous_scanline =
        out_rgba_values
        - (*out_width * bytes_per_channel);
    uint8_t * c_previous_scanline_previous_pixel = 
        b_previous_scanline - bytes_per_channel;
    
    for (uint32_t h = 0; h < *out_height; h++) {
        
        uint8_t filter_type = *decoded_stream++; 
        #ifndef DECODE_PNG_IGNORE_ASSERTS
        if (filter_type > 4) {
            #ifndef DECODE_PNG_SILENCE
            printf(
                "ERROR - unexpected filter_type of %u\n",
                filter_type);
            #endif
            *out_good = 0;
            if (decoded_stream) { free(decoded_stream_start); }
            if (headerless_compressed_data) { free(headerless_compressed_data_begin); };
            return;
        }
        #endif
        
        for (uint32_t w = 0; w < *out_width; w++) {
            // repeat this 3x or 4x, once for every byte in pixel
            // (RGB or R, G, B & A) 
            for (int _ = 0; _ < bytes_per_channel; _++) {
                // when a/b/c are out of bounds,
                // we have to use a 0 instead
                uint8_t a = w > 0 ?
                    *a_previous_pixel : 0; 
                uint8_t b = h > 0 ?
                    *b_previous_scanline : 0;
                uint8_t c = h > 0 && w > 0 ?
                    *c_previous_scanline_previous_pixel : 0;
                
                *rgba_at++ = undo_PNG_filter(
                    /* filter_type: */
                        filter_type,
                    /* original_value: */
                        *decoded_stream,
                    /* a_previous_pixel: */
                        a,
                    /* b_previous_scanline: */
                        b,
                    /* c_previous_scanline_previous_pixel: */
                        c);
                
                a_previous_pixel++;
                b_previous_scanline++;
                c_previous_scanline_previous_pixel++;
                
                decoded_stream++;
            }
        }
    }
    
    #ifndef DECODE_PNG_IGNORE_ASSERTS
    if(
        out_rgba_values_size
            != pixel_count * bytes_per_channel)
    {
        #ifndef DECODE_PNG_SILENCE
        printf(
            "ERROR - out_rgba_values_size of %u with pixel_count of %u and %u bytes_per_channel",
            out_rgba_values_size,
            pixel_count,
            bytes_per_channel);
        #endif
    }
    #endif
    
    // If we have less than 4 channels, forcibly convert to 4
    // channels of data by explicitly adding an alpha channel
    // of 255 to each pixel
    if (bytes_per_channel == 3) {
        // we don't need to assign new memory because our current
        // array already has space for 4 channels per pixel, it
        // just has 25% empty unitialized values at the end
        // We can start overwriting at the end (copying from
        // 25% away from the end) without ever overwriting
        // anything
        uint8_t * write_at =
            out_rgba_values +
                ((pixel_count * 4) - 1);
        uint8_t * read_at =
            out_rgba_values +
                (rgba_values_size - 1);
        
        for (
            uint32_t p = 0;
            p < pixel_count;
            p++)
        {
            *write_at-- = 255;
            *write_at-- = *read_at--;
            *write_at-- = *read_at--;
            *write_at-- = *read_at--;
        }
        
        assert(rgba_values_size >=
            pixel_count * 4);
        if (
            (rgba_values_size / 10) >
                pixel_count * 4)
        {
            #ifndef DECODE_PNG_SILENCE
            printf(
                "ERROR - you passed %u bytes of memory but the decoded PNG only takes %u bytes, this seems excessive\n",
                out_rgba_values_size,
                pixel_count * 4);
            #endif
            #ifndef DECODE_PNG_IGNORE_ASSERTS
            assert(0);
            #endif
        }
        
        rgba_values_size = pixel_count * 4;
    }
    
    free(decoded_stream_start);	
    free(headerless_compressed_data_begin);
    
    *out_good = 1;
}
