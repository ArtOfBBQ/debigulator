#include "inttypes.h"
#include "stdlib.h"
#include "inflate.h"

#define PNG_SILENCE
#define IGNORE_CRC_CHECKS
#define PNG_IGNORE_ASSERTS

// true/false/ignoreasserts are undef'd at EOF
#define true 1
#define false 0

#ifndef PNG_SILENCE
#include "stdio.h"
#endif

#ifndef PNG_IGNORE_ASSERTS
#include "assert.h"
#endif

typedef struct DecodedPNG {
    uint8_t * rgba_values;
    uint32_t rgba_values_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_count; // rgba_values_size / 4
    bool32_t good;
} DecodedPNG;

/*
PNG files include CRC 'cyclic redundancy checks', a kind
of checksum to make sure each block is valid data.
*/

#ifndef IGNORE_CRC_CHECKS
/*
Table of CRCs of all 8-bit messages.

The table itself is not found in the spec, but the spec
has a snippet of C code that generates it
*/
unsigned long crc_table[256] = {
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
unsigned long update_crc(
    unsigned long crc,
    unsigned char *buf,
    int len)
{
    unsigned long c = crc;
    int n;

    for (n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    
    return c;
}
#endif // IGNORE_CRC_CHECKS

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
void flip_endian(uint32_t * to_flip) {
    uint32_t flipping = *to_flip;
    
    uint32_t byte_1 = ((flipping >> 0) & 255);
    uint32_t byte_2 = ((flipping >> 8) & 255);
    uint32_t byte_3 = ((flipping >> 16) & 255);
    uint32_t byte_4 = ((flipping >> 24) & 255);
    
    flipping =
        byte_1 << 24 |
        byte_2 << 16 |
        byte_3 << 8  |
        byte_4 << 0;
    
    *to_flip = flipping;
}

bool32_t are_equal_strings(
    char * str1,
    char * str2,
    size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (str1[i] != str2[i]) {
            return false;
        }
    }
    
    return true;
}

/*
One of the reconstruction algorithms (see undo_PNG_filter below)
is a 'paeth predictor'

The PNG specification has sample code for the paeth predictor,
so I just copy pasted it here below.
*/
uint8_t compute_paeth_predictor(
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
uint8_t undo_PNG_filter(
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
        uint32_t u32_orig = (uint32_t)original_value;
        uint32_t u32_paeth = (uint32_t)computed_paeth;
        uint8_t return_value = original_value + computed_paeth;
        
	return return_value; 
    } else {
	#ifndef PNG_SILENCE
	printf(
	    "ERROR - unsupported filter type: %u. Expect 1-4\n",
	    filter_type);
	#endif	
	crash_program();
    }
    
    return 0;
}

DecodedPNG * decode_PNG(
    uint8_t * compressed_bytes,
    uint32_t compressed_bytes_size)
{
    DecodedPNG * return_value = NULL;
    return_value = malloc(sizeof(DecodedPNG));
    return_value->good = false;
    
    #ifndef PNG_IGNORE_ASSERTS
    assert(compressed_bytes_size > 0);
    #endif
    DataStream * entire_file = NULL;
    entire_file = malloc(sizeof(DataStream));
    entire_file->data = compressed_bytes;
    entire_file->size_left = compressed_bytes_size;
    entire_file->bits_left = 0;
    #ifndef PNG_IGNORE_ASSERTS
    assert(entire_file->bits_left == 0);
    #endif
    
    PNGSignature * png_signature =
        consume_struct(
            /* type: */ PNGSignature,
            /* from: */ entire_file);
    
    #ifndef PNG_IGNORE_ASSERTS
    assert(entire_file->bits_left == 0);
    #endif
    
    #ifndef PNG_SILENCE
    printf(
        "Signature's png_string (expecting 'PNG'): %c%c%c\n",
        png_signature->png_string[0],
        png_signature->png_string[1],
        png_signature->png_string[2]);
    #endif
    
    if (!are_equal_strings(
        /* string 1: */ png_signature->png_string,
        /* string 2: */ "PNG",
        /* string length: */ 3))
    {
        #ifndef PNG_SILENCE
        printf("aborting - not a PNG file\n");
        #endif
        return return_value;
    }
   
    #ifndef PNG_IGNORE_ASSERTS
    assert(entire_file->bits_left == 0);
    #endif
    free(png_signature);
    
    // these pointers are initted below
    uint8_t * compressed_data = NULL;
    uint8_t * compressed_data_begin = NULL;
    uint32_t compressed_data_stream_size = 0;
    uint8_t * decoded_stream = NULL;
    uint8_t * decoded_stream_start = NULL;
    unsigned int decompressed_size = 0;
    
    bool32_t found_first_IDAT = false;
    bool32_t found_IHDR = false;
    
    while (
        entire_file->size_left >= 12)
    {
        #ifndef PNG_SILENCE
        printf(
            "%lu bytes left in file, read another PNG chunk..\n",
            entire_file->size_left);
        #endif
        unsigned long running_crc = 0xffffffffL;
        PNGChunkHeader * chunk_header = consume_struct(
            /* type: */   PNGChunkHeader,
            /* buffer: */ entire_file);
        flip_endian(&chunk_header->length);
	
        if (!are_equal_strings(
            chunk_header->type,
            "IDAT",
            4)
            && found_first_IDAT)
        {
            #ifndef PNG_SILENCE
            printf("next chunk is not IDAT, ");
            printf("so all compressed data was collected.\n");
            #endif
            
            DataStream * compressed_data_stream =
                malloc(sizeof(DataStream));
            compressed_data_stream->data = compressed_data_begin;
            compressed_data_stream->size_left =
                compressed_data_stream_size;
            compressed_data_stream->bits_left = 0;
            
            #ifndef PNG_SILENCE
            printf(
                "created concatenated datastream of %lu bytes\n",
                compressed_data_stream->size_left);
            printf(
                "won't DEFLATE last 4 bytes because they're an ADLER-32 checksum...\n");
            #endif
            
            inflate(
                /* recipient: */
                    decoded_stream,
                /* recipient_size: */
                    decompressed_size,
                /* datastream: */
                    compressed_data_stream,
                /* compr_size_bytes: */
                    compressed_data_stream_size - 4);
	    
            free(compressed_data_begin);
            free(compressed_data_stream);
        }
        
        #ifndef IGNORE_CRC_CHECKS 
        running_crc = update_crc(
            /* crc: */ running_crc,
            /* buffer: */ (unsigned char *)&(chunk_header->type),
            /* length: */ 4);
        running_crc = update_crc(
            /* crc: */ running_crc,
            /* buffer: */ (unsigned char *)entire_file->data,
            /* length: */ chunk_header->length);
        running_crc = running_crc ^ 0xffffffffL;
        #endif
        
        #ifndef PNG_SILENCE 
        printf(
            "[%s] chunk (%u bytes)\n",
            chunk_header->type,
            chunk_header->length);
        #endif
        
        if (chunk_header->length >= entire_file->size_left) {
	    
            return_value->good = false;
	    #ifndef PNG_SILENCE 
	    printf(
		"failing to decode PNG, [%s] chunk of length %u is larger than remaining file size of %lu\n",
		chunk_header->type,
		chunk_header->length,
		entire_file->size_left);
	    #endif
            return return_value;
        }
        
        if (are_equal_strings(
            chunk_header->type,
            "PLTE",
            4))
        {
            if (decoded_stream == NULL) {

		#ifndef PNG_SILENCE 
		printf(
		    "failing to decode PNG. [%s] chunk should never appear before [IHDR] chunk\n",
		    chunk_header->type);
		#endif
                return_value->good = false;
                return return_value;
            }
        } else if (are_equal_strings(
            chunk_header->type,
            "IHDR",
            4))
        {
            found_IHDR = true;
            #ifndef PNG_IGNORE_ASSERTS
	    assert(entire_file->bits_left == 0);
            #endif
            IHDRBody * ihdr_body = consume_struct(
                /* type: */ IHDRBody,
                /* entire_file: */ entire_file);
            #ifndef PNG_IGNORE_ASSERTS
	    assert(entire_file->bits_left < 8);
            #endif
            flip_endian(&ihdr_body->width);
            flip_endian(&ihdr_body->height);
            return_value->width = ihdr_body->width;
            return_value->height = ihdr_body->height;
            
            decompressed_size =
                ihdr_body->width
                    * ihdr_body->height
                    * 5;
            
            // (below) These PNG setting assert the RGBA
            // color space.
            // There are many other formats (greyscale
            // illustrations etc.) found in .png files that we
            // are not supporting for now. 
            if (ihdr_body->bit_depth != 8
                || ihdr_body->color_type != 6)
            {
                #ifndef PNG_SILENCE
                printf("unsupported PNG bit depth / color type");
                #endif
                return_value->good = false;
                return return_value;
            }
           
            #ifndef PNG_SILENCE 
            printf(
                "\twidth: %u\n",
                ihdr_body->width);
            printf(
                "\theight: %u\n",
                ihdr_body->height);
            printf(
                "\tbit_depth: %u\n",
                ihdr_body->bit_depth);
            printf(
                "\tcolor_type: %u\n",
                ihdr_body->color_type);
            #endif
            
            #ifndef PNG_SILENCE
            if (ihdr_body->color_type != 6) {
                printf("error - only color type 6 is supported");
            } else {
                printf("\t\t(Truecolor with alpha)\n");
            }
            
            printf(
                "\tcompression_method: %u\n",
                ihdr_body->compression_method);
            printf(
                "\tfilter_method: %u\n",
                ihdr_body->filter_method);
            printf(
                "\tinterlace_method: %u\n",
                ihdr_body->interlace_method);
            #endif
            
            if (ihdr_body->filter_method != 0) {
		#ifndef PNG_SILENCE 
		printf(
		    "failing to decode PNG. filter method in [IHDR] chunk must be 0\n");
		#endif
                return_value->good = false;
                return return_value;
            }
            
            if (entire_file->size_left < 4
                || entire_file->bits_left != 0)
            {
		#ifndef PNG_SILENCE 
		printf(
		    "failing to decode PNG - file size left is %lu bytes, bits in bit buffer: %u\n",
		    entire_file->size_left,
		    entire_file->bits_left);
		#endif
                return_value->good = false;
                return return_value;
            }
	   
            #ifndef PNG_IGNORE_ASSERTS
	    assert(decoded_stream == NULL);
            #endif
	    
            decoded_stream = malloc(decompressed_size);
            // this copy (compressed_data) is necessary because
            // the data needed for DEFLATE is likely spread
            // across multiple chunks with useless header data
            // & checksums sprinkled in between
            // we'll concatenate all the data into compressed_data
            // first, then DEFLATE afterwards
            compressed_data = malloc(decompressed_size);
            compressed_data_begin = compressed_data;
            decoded_stream_start = decoded_stream;
	    free(ihdr_body);
        }  else if (are_equal_strings(
            chunk_header->type,
            "IDAT",
            4))
        {
            if (!found_IHDR
                || decoded_stream == NULL
                || compressed_data == NULL)
            {
		#ifndef PNG_SILENCE
		printf("failing to decode PNG - no [IHDR] chunk was found, yet already in [IDAT] chunk\n");
		#endif
                return_value->good = false;
                return return_value;
            }
            
            uint32_t chunk_data_length =
                chunk_header->length;
            
            if (!found_first_IDAT) {
                #ifndef PNG_SILENCE
                printf("\t1st IDAT chunk, read zlib headers..\n");
                #endif
                found_first_IDAT = true;
                
                ZLIBHeader * zlib_header =
                    consume_struct(
                        /* type: */ ZLIBHeader,
                        /* from: */ entire_file);
                chunk_data_length -= sizeof(ZLIBHeader);
               
                // to mask the rightmost 4 bits we need 00001111 
                //                                          8421
                //                                    8+4+2+1=15
                // to isolate the leftmost 4, we need to
                // shift right by 4
                // (I guess the new padding bits on the left
                // are 0 by default)
                uint8_t compression_method =
                    zlib_header->zlibmethodflags & 15;
                uint8_t compression_info =
                    zlib_header->zlibmethodflags >> 4;
                #ifndef PNG_SILENCE
                printf(
                    "\t\tcompression method: %u\n",
                    compression_method);
                printf(
                    "\t\tcompression info: %u\n",
                    compression_info);
                #endif
                if (compression_method != 8) {
                    return_value->good = false;
                    return return_value;
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
                uint16_t full_check_value =
                    (uint16_t)zlib_header->additionalflags;
                full_check_value =
                    full_check_value
                    | (zlib_header->zlibmethodflags << 8);
                #ifndef PNG_SILENCE
                printf(
                    "\t\tfull 16-bit check val (use FCHECK): %u - ",
                    full_check_value);
                printf(
                    full_check_value % 31 == 0 ? "OK\n" : "ERR\n");
                #endif
                
                if (full_check_value == 0
                    || full_check_value % 31 != 0)
                {
                    return_value->good = false;
                    return return_value;
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
                    zlib_header->additionalflags >> 5 & 1;
                
                if (FDICT) {
                    #ifndef PNG_SILENCE
                    printf("\t\tdiscarding FDICT... (4 bytes)\n");
                    #endif
                    discard_bits(entire_file, 32);
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
                uint8_t FLEVEL =
                    zlib_header->additionalflags >> 6 & 3;
                if (FDICT != 0) {
                    return_value->good = false;
                    return return_value;
                }
                
                #ifndef PNG_SILENCE
                printf("\t\tFDICT: %u\n", FDICT);
                printf("\t\tFLEVEL: %u\n", FLEVEL);
                printf("\t\tread compressed data...\n");
                printf(
                    "\t\tprepped recipient of %u bytes...\n",
                    decompressed_size);
                #endif
                
                free(zlib_header);
            }
            
            #ifndef PNG_SILENCE
            printf(
                "\t\tcopying %u bytes to compressed stream...\n",
                chunk_data_length);
            #endif
            if (entire_file->bits_left != 0) {
                return_value->good = false;
                return return_value;
            }
            
            for (uint32_t _ = 0; _ < chunk_data_length; _++)
            {
                *compressed_data++ =
                    *(uint8_t *)entire_file->data++;
                compressed_data_stream_size++;
                entire_file->size_left--;
            }    
        }
        else if (are_equal_strings(
            chunk_header->type,
            "IEND",
            4))
        {
            if (decoded_stream == NULL) {
                return_value->good = false;
                return return_value;
            }
            
            // handle image end header
            #ifndef PNG_SILENCE
            printf("found IEND header\n");
            #endif
        }
        else if ((char)chunk_header->type[0] > 'Z')
        {
            #ifndef PNG_SILENCE
            printf(
                "\tskipping unhandled non-critical chunk...\n");
            #endif
            entire_file->data += chunk_header->length;
            entire_file->size_left -= chunk_header->length;
        } else {
            #ifndef PNG_SILENCE
            printf(
                "ERROR: unhandled critical chunk header: %s\n",
                chunk_header->type);
            #endif
            return return_value;
        }
        
        if (entire_file->size_left < 4
            || entire_file->bits_left != 0)
	{
	    #ifndef PNG_SILENCE
	    printf(
		"failed to decode PNG - unexpected remaining file size of %lu and %u bits in buffer\n",
		entire_file->size_left,
		entire_file->bits_left);
	    #endif
            return_value->good = false;
            return return_value;
        }
        
        PNGFooter * block_footer =
            consume_struct(
                PNGFooter,
                entire_file);
        
        #ifndef IGNORE_CRC_CHECKS
        flip_endian(&block_footer->CRC);
        if (running_crc != block_footer->CRC) {
            #ifndef PNG_SILENCE
            printf("ERROR: CRC checksum mismatch - this PNG file is corrupted.\n");
            #endif
            return return_value;
        } else {
            #ifndef PNG_SILENCE
            printf("CRC checksum match: OK\n");
            #endif
        }
        #endif
	
	free(chunk_header);	
	free(block_footer);
    }
    
    // end of "while size file > pngchunkheader" loop
    
    // we've uncompressed the data using DEFLATE
    //
    // now let's undo the filtering methods
    // we already asserted the IHDR filter method was 0
    // , filter_type is something else and the first num
    // in the uncompressed datastream
    // "filters" could have been called "transforms",
    // not sure how it's a filter
    
    free(entire_file);
    uint32_t filter_type = *decoded_stream_start;
    #ifndef PNG_SILENCE
    printf(
        "\n\nreconstructing (un-doing PNG filters)...\n");
    #endif
    
    uint32_t pixel_count =
        return_value->width
            * return_value->height;
    return_value->pixel_count = pixel_count;
    return_value->rgba_values = malloc(pixel_count * 4);
    return_value->rgba_values_size = 0;
    
    decoded_stream = decoded_stream_start;
    uint8_t * rgba_at = return_value->rgba_values;
    
    
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
    uint8_t * a_previous_pixel = return_value->rgba_values - 4;
    uint8_t * b_previous_scanline =
        return_value->rgba_values
        - (return_value->width * 4);
    uint8_t * c_previous_scanline_previous_pixel = 
        b_previous_scanline - 4;
    
    for (int h = 0; h < return_value->height; h++) {
       	
        uint8_t filter_type = *decoded_stream++; 
        
        for (int w = 0; w < return_value->width; w++) {
	    
	    // repeat this 4x, once for every byte in pixel
            // (R, G, B & A) 
            for (int _ = 0; _ < 4; _++) {
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
                
		return_value->rgba_values_size++;
                a_previous_pixel++;
                b_previous_scanline++;
                c_previous_scanline_previous_pixel++;
                
		decoded_stream++;
            }
        }
    }
    
    #ifndef PNG_IGNORE_ASSERTS
    assert(
        return_value->rgba_values_size
            == return_value->pixel_count * 4);
    #endif
    
    return_value->good = true;
    free(decoded_stream_start);
    
    return return_value;
}

#undef true
#undef false
#undef PNG_IGNORE_ASSERTS

