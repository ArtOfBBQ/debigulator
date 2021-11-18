#include "stdio.h"
#include "inttypes.h"
#include "stdlib.h"
#include "assert.h"
#include "deflate.h"

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
(13 data bytes total).
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
*/
typedef struct IDATHeader {
    uint8_t zlibmethodflags;
    uint8_t additionalflags;
} IDATHeader;
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

void copy_memory(
    void * from,
    void * to,
    size_t size)
{
    uint8_t * fromu8 = (uint8_t *)from;
    uint8_t * tou8 = (uint8_t *)to;
    
    while (size > 0) {
        *tou8 = *fromu8;
        fromu8++;
        tou8++;
        size--;
    }
}

bool32_t are_equal_strings(
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

// Grab data from the front of a buffer & advance pointer
#define consume_struct(type, from) (type *)consume_chunk(from, sizeof(type))
uint8_t * consume_chunk(
    EntireFile * from,
    size_t size_to_consume)
{
    assert(from->bits_left == 0);
    assert(from->size_left >= size_to_consume);
    
    uint8_t * return_value = malloc(size_to_consume);
    
    copy_memory(
        /* from: */   from->data,
        /* to: */     return_value,
        /* size: */   size_to_consume);
    
    from->data += size_to_consume;
    from->size_left -= size_to_consume;
    
    return return_value;
}

int main(int argc, const char * argv[]) 
{
    printf("Hello .png files!\n");
    if (argc != 2) {
        printf("Please supply 1 argument (png file name)\n");
        printf("Got:");
        for (int i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        return 1;
    }
    
    printf("Inspecting file: %s\n", argv[1]);
    
    FILE * imgfile = fopen(
        argv[1],
        "rb");
    fseek(imgfile, 0, SEEK_END);
    size_t fsize = ftell(imgfile);
    fseek(imgfile, 0, SEEK_SET);
    
    uint8_t * buffer = malloc(fsize);
    EntireFile * entire_file = malloc(sizeof(EntireFile));
    
    size_t bytes_read = fread(
        /* ptr: */
            buffer,
        /* size of each element to be read: */
            1, 
        /* nmemb (no of members) to read: */
            fsize,
        /* stream: */
            imgfile);
    printf("bytes read: %zu\n", bytes_read);
    fclose(imgfile);
    assert(bytes_read == fsize);
    
    entire_file->data = buffer;
    entire_file->size_left = bytes_read;
    
    PNGSignature * png_signature = consume_struct(
        /* type: */ PNGSignature,
        /* from: */ entire_file);
    
    printf(
        "Signature's png_string (expecting 'PNG'): %c%c%c\n",
        png_signature->png_string[0],
        png_signature->png_string[1],
        png_signature->png_string[2]);
    
    if (!are_equal_strings(
        /* string 1: */ png_signature->png_string,
        /* string 2: */ "PNG",
        /* string length: */ 3))
    {
        printf("aborting - not a PNG file\n");
        return 1;
    }
    
    // The first chunk must be IHDR
    assert(entire_file->size_left >= sizeof(PNGChunkHeader));
    PNGChunkHeader * ihdr_header = consume_struct(
        /* type  : */ PNGChunkHeader,
        /* buffer: */ entire_file);
    if (!are_equal_strings(
            ihdr_header->type,
            "IHDR",
            4))
    {
        printf(
            "Incomplete PNG: Missing img header (IHDR) chunk\n");
        return 1;
    } else {
        printf("[IHDR] chunk\n");
    }
    
    assert(sizeof(ihdr_header) == 8);
    flip_endian(&ihdr_header->length);
    assert(ihdr_header->length == 13);
    bool32_t supported = true;
    IHDRBody * ihdr_body = consume_struct(
        /* type: */ IHDRBody,
        /* entire_file: */ entire_file);
    
    flip_endian(&ihdr_body->width);
    flip_endian(&ihdr_body->height);
    
    // (below) These PNG setting assert the RGBA color space.
    // There are many other formats (greyscale
    // illustrations etc.) found in .png files that we
    // are not supporting for now. 
    if (ihdr_body->bit_depth != 8
        || ihdr_body->color_type != 6)
    {
        printf("unsupported PNG bit depth / color type");
        return 1;
    }
    
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
    assert(ihdr_body->filter_method == 0);
    printf(
        "\tinterlace_method: %u\n",
        ihdr_body->interlace_method);
    
    // ignore a 4-byte CRC checksum
    printf("ignoring 4-byte CRC checksum...\n");
    assert(entire_file->size_left >= 4);
    assert(entire_file->bits_left == 0);
    entire_file->data += 4;
    entire_file->size_left -= 4;
    printf("CRC checksum ditched\n");
     
    // these pointers are initted below
    uint8_t * pixels = 0;
    uint8_t * pixels_start = 0;
    if (supported) {
        pixels = malloc(
            ihdr_body->height
            * ihdr_body->width
            * 4);
        pixels_start = pixels;
    } else {
        printf("unsupported PNG format\n");
        return 1;
    }
    
    while (
        entire_file->size_left >= sizeof(PNGChunkHeader))
    {
        PNGChunkHeader * chunk_header = consume_struct(
            /* type: */   PNGChunkHeader,
            /* buffer: */ entire_file);
        assert(sizeof(chunk_header) == 8);
        flip_endian(&chunk_header->length);
        
        printf(
            "[%s] chunk (%u bytes)\n",
            chunk_header->type,
            chunk_header->length);
        
        if (are_equal_strings(
            chunk_header->type,
            "PLTE",
            4))
        {
            // handle palette header
            break;
        } else if (are_equal_strings(
            chunk_header->type,
            "IDAT",
            4))
        {
            // handle image data header
            printf("\tfound IDAT (image data) header...\n");
           
 
            // (IDATHeader *)entire_file->data;
            IDATHeader * idat_header =
                consume_struct(
                    /* type: */ IDATHeader,
                    /* from: */ entire_file);
            
            // to mask the rightmost 4 bits we need 00001111 
            //                                          8421
            //                                    8+4+2+1=15
            // to isolate the leftmost 4, we need to
            // shift right by 4
            // (I guess the new padding bits on the left
            // are 0 by default)
            uint8_t compression_method =
                idat_header->zlibmethodflags & 15;
            uint8_t compression_info =
                idat_header->zlibmethodflags >> 4;
            printf(
                "\t\tcompression method: %u\n",
                compression_method);
            printf(
                "\t\tcompression info: %u\n",
                compression_info);
            
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
                (uint16_t)idat_header->additionalflags;
            full_check_value =
                full_check_value
                | (idat_header->zlibmethodflags << 8);
            printf(
                "\t\tfull 16-bit check val (use FCHECK): %u - ",
                full_check_value);
            printf(
                full_check_value % 31 == 0 ? "OK\n" : "ERR\n");
            assert(full_check_value != 0);
            assert(full_check_value % 31 == 0);
            
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
                idat_header->additionalflags >> 5 & 1;
            
            /*
            7th & 8th bit is FLEVEL
            FLEVEL (Compression level)
            The "deflate" method (CM = 8) sets these flags as
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
                idat_header->additionalflags >> 6 & 3;
            printf("\t\tFDICT: %u\n", FDICT);
            printf("\t\tFLEVEL: %u\n", FLEVEL);

            assert(FDICT == 0);
            
            printf("\t\t\tread compressed data...\n");
            
            /*
            Each block of compressed data begins with 3 header
            bits containing the following data:
            
            1st bit         BFINAL
            next 2 bits     BTYPE
            
            Note that the header bits do not necessarily begin
            on a byte boundary, since a block does not
            necessarily occupy an integral number of bytes.
            
            BFINAL is set if and only if this is the last
            block of the data set.
            
            BTYPE specifies how the data are compressed,
            as follows:
            
            00 - no compression
            01 - compressed with fixed Huffman codes
            10 - compressed with dynamic Huffman codes
            11 - reserved (error) 
            */
            uint8_t BFINAL = consume_bits(
                /* buffer: */ entire_file,
                /* size  : */ 1);
            
            assert(BFINAL < 2);
            printf(
                "\t\t\tBFINAL (flag for final block): %u\n",
                BFINAL);
           
            // Jelle: I stumbled my way into using
            // reverse_bit_order here (because it appears to
            // solve the problem accidentally) casey does
            // no such thing 
            // uint8_t BTYPE = reverse_bit_order(
            //     consume_bits(
            //         /* buffer: */ entire_file,
            //         /* size  : */ 2),
            //     2);
            uint8_t BTYPE =  consume_bits(
                /* buffer: */ entire_file,
                /* size  : */ 2);
            
            char * btype_description;
            
            switch (BTYPE) {
                case (0):
                    printf("\t\t\tBTYPE 0 - No compression\n");
                    
                    // spec says to ditch remaining bits
                    if (entire_file->bits_left < 8) {
                        printf(
                            "\t\t\tditching a byte with %u%s\n",
                            entire_file->bits_left,
                            " bits left...");
                        entire_file->bits_left = 0;
                        // the data itself was already incr.
                        // when bits were consumed
                        // entire_file->data += 1;
                        // entire_file->bits_left = 8;
                        // entire_file->bit_buffer =
                            // *(uint8_t *)entire_file->data;
                    }
                    
                    // read LEN and NLEN (see next section)
                    uint16_t LEN =
                        *(int16_t *)entire_file->data; 
                    entire_file->data += 2;
                    entire_file->size_left -= 2;
                    printf(
                        "\t\t\tcompr. block has LEN: %u bytes\n",
                        LEN);
                    uint16_t NLEN =
                        *(uint16_t *)entire_file->data; 
                    entire_file->data += 2;
                    entire_file->size_left -= 2;
                    printf(
                        "\t\t\tcompr. blck has NLEN:%d byte\n",
                        NLEN);
                    printf(
                        "\t\t\t1's complement of LEN was: %u\n",
                        ~LEN);
                    assert(NLEN == ~LEN);
                    
                    // TODO: handle no compression
                    // copy LEN bytes of data to output
                    
                    break;
                case (1):
                    printf("\t\t\tBTYPE 1 - Fixed Huffman\n");
                    printf("\t\t\tPNG format unsupported.");
                   
                    // TODO: handle fixed huffman 
                    return 1;
                    
                    break;
                case (2):
                    printf("\t\t\tBTYPE 2 - Dynamic Huffman\n");
                    printf("\t\t\tRead code trees...\n");
                    
                    /*
                    The Huffman codes for the two alphabets
                    appear in the block immediately after the
                    header bits and before the actual compressed
                    data, first the literal/length code and then
                    the distance code. Each code is defined by a
                    sequence of code lengths.
                    */
                    
                    // 5 Bits: HLIT (huffman literal)
                    // number of Literal/Length codes - 257
                    // (257 - 286)
                    uint32_t HLIT = consume_bits(
                        /* from: */ entire_file,
                        /* size: */ 5)
                            + 257;
                    printf(
                        "\t\t\tHLIT : %u (expect 257-286)\n",
                        HLIT);
                    assert(HLIT >= 257 && HLIT <= 286);
                    
                    // 5 Bits: HDIST (huffman distance?)
                    // # of Distance codes - 1
                    // (1 - 32)
                    uint32_t HDIST = consume_bits(
                        /* from: */ entire_file,
                        /* size: */ 5)
                            + 1;
                    
                    printf(
                        "\t\t\tHDIST: %u (expect 1-32)\n",
                        HDIST);
                    assert(HDIST >= 1 && HDIST <= 32);
                    
                    
                    // 4 Bits: HCLEN (huffman code length)
                    // # of Code Length codes - 4
                    // (4 - 19)
                    uint32_t HCLEN = consume_bits(
                        /* from: */ entire_file,
                        /* size: */ 4)
                            + 4;
                    
                    printf(
                        "\t\t\tHCLEN: %u (4-19 vals of 0-6)\n",
                        HCLEN);
                    assert(HCLEN >= 4 && HCLEN <= 19);
                    
                    // This is a fixed swizzle (order of elements
                    // in an array) that's agreed upon in the
                    // specification.
                    // I think they did this because we usually
                    // don't need all 19, and they thought
                    // 15 would be the least likely one to be
                    // necessary, 1 the second least likely,
                    // etc.
                    // When they only pass 7 values, we are
                    // expected to have initted the other ones
                    // to 0 (I guess), and that allows them to
                    // skip mentioning the other 13 ones and
                    // save 12 x 3 = 36 bits
                    // So it's basically compressing by
                    // truncating
                    // (array size: 19)
                    // I never would have imagined people go this
                    // far to save 36 bits of disk space
                    uint32_t swizzle[] =
                        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                         11, 4, 12, 3, 13, 2, 14, 1, 15};
                    
                    // read (HCLEN + 4) x 3 bits
                    // these are code lengths for the code length
                    // dictionary,
                    // and they'll come in swizzled order
                    
                    uint32_t HCLEN_table[
                        NUM_UNIQUE_CODELENGTHS] = {};
                    printf("\t\t\tReading raw code lengths:\n");
                    
                    assert(HCLEN < NUM_UNIQUE_CODELENGTHS);
                    for (uint32_t i = 0; i < HCLEN; i++) {
                        assert(swizzle[i] <
                            NUM_UNIQUE_CODELENGTHS);
                        HCLEN_table[swizzle[i]] =
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 3);
                        assert(HCLEN_table[swizzle[i]] <= 7);
                        assert(HCLEN_table[swizzle[i]] >= 0);
                    }
                    
                    /*
                    We now have some values in HCLEN_table,
                    but these are themselves 'compressed'
                    and need to be unpacked
                    */ 
                    printf("\t\t\tUnpck codelengths table...\n");
                   
                    // TODO: should I do NUM_UNIQUE_CODELGNTHS
                    // or only HCLEN? 
                    // HuffmanEntry * codelengths_huffman =
                    //     unpack_huffman(
                    //         /* array:     : */
                    //             HCLEN_table,
                    //         /* array_size : */
                    //             NUM_UNIQUE_CODELENGTHS);
                    HuffmanEntry * codelengths_huffman =
                        unpack_huffman(
                            /* array:     : */
                                HCLEN_table,
                            /* array_size : */
                                NUM_UNIQUE_CODELENGTHS);
                    
                    for (
                        int i = 0;
                        i < NUM_UNIQUE_CODELENGTHS;
                        i++)
                    {
                        if (
                            codelengths_huffman[i].code_length
                                == 0)
                        {
                            continue;
                        }
                        
                        assert(
                          codelengths_huffman[i].value >= 0);
                        assert(
                          codelengths_huffman[i].value < 19);
                    }
                    
                    /*
                    Now we have an unpacked table with code
                    lengths from 0 to 18. Each code length
                    implies a different action, see below.
                    
                    We need to use this 'code length' table
                    to create 2 new tables of codes before we
                    can finally fully unpack:
                    - HLIT + 257 code lengths for the
                      literal/length alphabet, encoded using
                      the code length Huffman code
                    - HDIST + 1 code lengths for the distance
                      alphabet, encoded using the code length
                      Huffman code
                    */
                    
                    uint32_t len_i = 0;
                    uint32_t two_dicts_size = HLIT + HDIST;
                    printf(
                        "\t\t\tdecoding lit/len using %s...\n",
                        "unpacked code lengths table");
                    
                    uint32_t * litlendist_table = malloc(
                        sizeof(uint32_t) * two_dicts_size);
                    
                    while (len_i < two_dicts_size) {
                        uint32_t encoded_len = huffman_decode(
                            /* dict: */
                                codelengths_huffman,
                            /* dictsize: */
                                NUM_UNIQUE_CODELENGTHS,
                            /* raw data: */
                                entire_file);
                        
                        if (encoded_len <= 15) {
                            litlendist_table[len_i] =
                                encoded_len;
                            len_i++;
                        } else if (encoded_len == 16) {
                        /*
                        !COPIED FROM SPECIFICATION!
                        16: Copy previous code length 3-6 times.
                            2 extra bits for repeat length
                            (0 = 3, ... , 3 = 6)
                        */
                            uint32_t extra_bits_repeat =
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 2);
                            uint32_t repeats =
                                extra_bits_repeat + 3;
                            
                            assert(repeats >= 3);
                            assert(repeats <= 6);
                            
                            for (
                                int i = 0;
                                i < repeats;
                                i++)
                            {
                                litlendist_table[len_i] =
                                    litlendist_table[len_i - 1];
                                len_i++;
                            }
                        } else if (encoded_len == 17) {
                        /*
                        !COPIED FROM SPECIFICATION!
                        17: Repeat a code length of 0 for 3 - 10
                            times.
                            3 extra bits for length
                        */
                            uint32_t extra_bits_repeat =
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 3);
                            uint32_t repeats =
                                extra_bits_repeat + 3;
                            
                            assert(repeats >= 3);
                            assert(repeats < 11);
                            
                            for (
                                int i = 0;
                                i < repeats;
                                i++)
                            {
                                litlendist_table[len_i] = 0;
                                len_i++;
                            }
                            
                        } else if (encoded_len == 18) {
                        /*
                        !COPIED FROM SPECIFICATION!
                        18: Repeat a code length of 0 for
                            11 - 138 times
                            7 extra bits for length
                        */
                            uint32_t extra_bits_repeat =
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 7);
                            uint32_t repeats =
                                extra_bits_repeat + 11;
                            assert(repeats >= 11);
                            assert(repeats < 139);
                            
                            for (
                                int i = 0;
                                i < repeats;
                                i++)
                            {
                                litlendist_table[len_i] = 0;
                                len_i++;
                            }
                        } else {
                            printf(
                                "ERROR : encoded_len %u\n",
                                encoded_len);
                            return 1;
                        }
                    }
                    
                    printf("\t\t\tfinished reading two dicts\n");
                    assert(len_i == two_dicts_size);
                    
                    HuffmanEntry * litlen_huffman =
                        unpack_huffman(
                            /* array:     : */
                                litlendist_table,
                            /* array_size : */
                                HLIT);
                    printf("\t\tunpacked litlen_huffman\n");
                    
                    HuffmanEntry * dist_huffman =
                        unpack_huffman(
                            /* array:     : */
                                litlendist_table + HLIT,
                            /* array_size : */
                                HDIST);
                    printf("\t\tunpacked dist_huffman\n");
                    
                    // Next, we need to read the actual data
                    // and decode it using the 'litlen'
                    // dictionary, which should yield a value
                    // from 0-285
                    // the values 0-255 are copied as literals
                    // the value 256 means 'end of block'
                    // the values 256-285 are length/distances
                    // and will often need 'extra bits' to
                    // determine the exact length
                    while (entire_file->size_left > 0) {
                        // this consumes from entire_file
                        // so we'll eventually hit 256 and break
                        uint32_t litlenvalue = huffman_decode(
                            /* dict: */
                                litlen_huffman,
                            /* dictsize: */
                                HLIT,
                            /* raw data: */
                                entire_file);
                        if (litlenvalue < 256) {
                            *pixels++ =
                                (uint8_t)(litlenvalue & 255);
                        } else if (litlenvalue > 256) {
                            
                            assert(litlenvalue < 286);
                            uint32_t i = litlenvalue - 257;
                            assert(i < 29);
                            assert(
                                length_extra_bits_table[i].value
                                    == litlenvalue);
                            uint32_t extra_bits =
                                length_extra_bits_table[i]
                                    .num_extra_bits;
                            uint32_t length =
                                length_extra_bits_table[i]
                                    .base_decoded;
                            if (extra_bits > 0) {
                                length += consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ extra_bits);
                            }
                            assert(
                                length >=
                                length_extra_bits_table[i]
                                    .base_decoded);
                        } else {
                            assert(litlenvalue == 256);
                            printf("\t\tend of ltln found!\n");
                            // TODO: figure out what to do
                            // with any remaining bits
                            entire_file->bits_left = 0;
                            break;
                        }
                        
                        uint32_t distvalue = huffman_decode(
                            /* dict: */
                                dist_huffman,
                            /* dictsize: */
                                HDIST,
                            /* raw data: */
                                entire_file);
                        
                        assert(distvalue <= 29);
                        assert(
                            dist_extra_bits_table[distvalue]
                                .value
                                    == distvalue);
                        uint32_t extra_bits =
                            dist_extra_bits_table[distvalue]
                                .num_extra_bits;
                        uint32_t dist =
                            dist_extra_bits_table[distvalue]
                                .base_decoded;
                        if (extra_bits > 0) {
                            dist += consume_bits(
                                /* from: */ entire_file,
                                /* size: */ extra_bits);
                        }
                    }
                    
                    break;
                case (3):
                    printf("\t\t\tBTYPE 3 - reserved (error)\n");
                    assert(1 == 2);
                    break;
                default:
                    printf(
                        "\t\t\tERROR: BTYPE of %u%s\n",
                        BTYPE,
                        " was not in the Deflate specification");
            }
            
            assert(BTYPE < 3);
        }
        else if (are_equal_strings(
            chunk_header->type,
            "IEND",
            4))
        {
            // handle image end header
            printf("found IEND header");
            break;
        }
        else if ((char)chunk_header->type[0] > 'Z')
        {
            uint32_t skip = chunk_header->length;
            printf(
                "\tskip unhandled noncrit chunk (type :%s)\n",
                chunk_header->type);
            
            entire_file->data += skip;
            
            assert(skip <= entire_file->size_left); 
            entire_file->size_left -= skip;
            printf("\tskipped noncritical chunk\n");
        } else {
            printf(
                "ERROR: unhandled critical chunk header: %s\n",
                chunk_header->type);
            return 1;
        }
         
        // ignore a 4-byte CRC checksum
        printf("ignoring 4-byte CRC checksum...\n");
        assert(entire_file->size_left >= 4);
        assert(entire_file->bits_left == 0);
        entire_file->data += 4;
        entire_file->size_left -= 4;
        printf("CRC checksum ditched\n");
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
    printf("about to read filter method\n");
    uint32_t filter_type = *pixels_start;
    printf(
        "\t\tfilter method: %u\n",
        filter_type);
    
    return 0;
}

