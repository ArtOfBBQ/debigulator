#include "stdio.h"
#include "inttypes.h"
#include "stdlib.h"
#include "assert.h"

typedef int32_t bool32_t;
#define false 0
#define true 1

#define NUM_UNIQUE_CODELENGTHS 19

/*
I'm not sure how to deal with this in C,
I just want an array with the size stored
*/
typedef struct EntireFile {
    void * data;
    
    uint8_t bits_left;
    uint32_t bit_buffer;
    
    size_t size_left;
} EntireFile;

/*
A PNG file starts with an 8-byte signature
All of these seem like ancient artifacts to me,
except for bytes 2 to 4, which read 'PNG'
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
A PNG Chunk header
Length           (4 bytes)
Chunk type/name  (4 bytes)

Will be followed by the data
Chunk data       (length bytes)
CRC checksum     (4 bytes)
*/
typedef struct PNGChunkHeader {
    uint32_t length;     // big endian
    char type[4];        // 1st letter upper = critical chunk
} PNGChunkHeader;


/*
IHDR must be the first chunk; it contains (in this order)
the image's width (4 bytes);
height (4 bytes);
bit depth (1 byte, values 1, 2, 4, 8, or 16);
color type (1 byte, values 0, 2, 3, 4, or 6);
compression method (1 byte, value 0);
filter method (1 byte, value 0);
and interlace method (1 byte, values 0 "no interlace" or 1 "Adam7 interlace") 
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

/* IDAT chunks have their own header

*/
typedef struct IDATHeader {
    uint8_t zlibmethodflags;
    uint8_t additionalflags;
} IDATHeader;

typedef struct IDATFooter {
    uint32_t checkvalue;
} IDATFooter;
#pragma pack(pop)

typedef struct HuffmanPair {
    uint32_t key;
    uint32_t code_length;
    uint32_t value;
} HuffmanPair;

/*
PNG files have many 4-byte big endian values
We need to able to flip them to little endian

Bitwise operations reminder:
Operator	Effect
&               Bitwise AND operator
|               Bitwise OR operator
^               Bitwise exclusive OR operator
~               Binary One’s Complement Operator
<<              Left shift 
>>              Right shift
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

/*
grab data from buffer & advance pointer this is for grabbing
only bits, use 'consume_struct' for bytes

Data elements are packed into bytes in order of increasing bit
number within the byte, i.e., starting with the least-significant
bit of the byte.
* Data elements other than Huffman codes are packed
  starting with the least-significant bit of the data
  element.
* Huffman codes are packed starting with the most-
  significant bit of the code.

Jelle: That's so fucked up. Why would you store 2 different
types of data in a different order just for kicks? :/

In other words, if one were to print out the compressed data as
a sequence of bytes, starting with the first byte at the
*right* margin and proceeding to the *left*, with the most-
significant bit of each byte on the left as usual, one would be
able to parse the result from right to left, with fixed-width
elements in the correct MSB-to-LSB order and Huffman codes in
bit-reversed order (i.e., with the first bit of the code in the
relative LSB position).

Jelle: IMHO this means this:
let's say you have the number 00001001 (9)

->
FOR HUFFMAN CODES
you consume 2 bits and get: 10 which is 2
you consume another 2 bits and get: 01 which is 1
my function does this
->
FOR EVERYTHING ELSE
you consume 2 bits and get: 01 which is 1
you consume another 2 bits and get: 10 which is 2
i have no function yet that does this
*/
uint8_t consume_bits(
    EntireFile * from,
    uint8_t bits_to_consume)
{
    assert(bits_to_consume < 33);
    
    uint32_t return_value = 0;
    
    while (bits_to_consume > 0) {
        if (from->bits_left > 0) {
            return_value = return_value << 1;
            return_value =
                return_value
                | (from->bit_buffer & 1);
            from->bit_buffer = from->bit_buffer >> 1;
            from->bits_left -= 1;
            bits_to_consume -= 1;
        } else {
            from->bit_buffer = *((uint32_t *)from->data);
            from->bits_left = 32;
            
            from->size_left -= 4;
            from->data += 4;
        }
    }
    
    return return_value;
}

uint8_t reverse_bit_order(
    uint8_t original,
    uint8_t bit_count)
{
    assert(bit_count > 1);
    assert(bit_count < 9);
    
    uint8_t return_value = 0;
    
    float middle = (bit_count - 1.0f) / 2.0f;
    assert(middle > 0);
    assert(middle <= 3.5f);
    
    for (uint8_t i = 0; i < bit_count; i++) {
        float dist_to_middle = middle - i;
        assert(dist_to_middle < 4);
        
        uint8_t target_pos = i + (dist_to_middle * 2);
        
        assert(target_pos >= 0);
        assert(target_pos < 8);
        
        return_value =
            return_value |
                (((original >> i) & 1) << target_pos);
    }
    
    return return_value;
}

// Grab data from the front of a buffer & advance pointer
#define consume_struct(type, from) (type *)consume_chunk(from, sizeof(type))
uint8_t * consume_chunk(
    EntireFile * from,
    size_t size_to_consume)
{
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

uint32_t huffman_decode(
    HuffmanPair * dict,
    uint32_t dictsize,
    EntireFile * datastream)
{
    printf("huffman_decode...\n");
    uint32_t raw = 0;
    int found_at = -1;
    int bitcount = 0;
    
    while (found_at == -1 && bitcount < 15) {
        bitcount += 1;
        raw = raw
            |
            (consume_bits(
                /* from: */ datastream,
                /* size: */ 1)
                    << bitcount);
        
        printf("new raw: %u\n", raw);
        
        for (int i = 0; i < dictsize; i++) {
            if (dict[i].key == raw)
            {
                found_at = i;
                break;
            }
        }
    }
    
    if (found_at < 0) {
        printf(
            "failed to find raw value:%u in dict\n",
            raw);
    } else {
        printf(
            "found raw: %u to mean value: %u\n",
            raw,
            dict[found_at].value);
    }
    
    assert(found_at >= 0);
    assert(found_at < dictsize);
    
    printf("returning %u\n", dict[found_at].value);
    
    return dict[found_at].value;
};

HuffmanPair * unpack_huffman(
    uint32_t * array,
    uint32_t array_size)
{
    HuffmanPair * unpacked_dict = malloc(
        sizeof(HuffmanPair) * array_size);
    // initialize dict
    for (int i = 0; i < array_size; i++) {
        unpacked_dict[i].value = i;
        unpacked_dict[i].code_length = array[i];
    }
    
    // this is straight from the deflate spec
    
    // 1) Count the number of codes for each code length.  Let
    // bl_count[N] be the number of codes of length N, N >= 1.
    uint32_t * bl_count = malloc(64);
    for (int i = 0; i < 64; i++) {
        bl_count[i] = 0;
    }
    
    for (int i = 0; i < array_size; i++) {
        bl_count[array[i]] += 1;
    }
    
    // 2) Find the numerical value of the smallest code for each
    //    code length:
    uint32_t * smallest_code = malloc(200);
    uint32_t last_code = 0;
    bl_count[0] = 0;
    for (uint32_t bits = 1; bits < 32; bits++) {
        if (bl_count[bits] > 0) {
            last_code += 1;
            while (last_code < (1 << (bits - 1))) {
                last_code <<= 1;
            }
            smallest_code[bits] = last_code;
            assert(smallest_code[bits] < (1 << bits));
        } else {
            smallest_code[bits] = 0;
        }
    }
    
    // 3) Assign numerical values to all codes, using
    //    consecutive values for all codes of the same length
    //    with the base values determined at step 2. Codes that
    //    are never used (which have a bit length of zero) must
    //    not be assigned a value.
    for (uint32_t n = 0; n < array_size; n++) {
        uint32_t len = unpacked_dict[n].code_length;
        
        if (len != 0) {
            unpacked_dict[n].key = smallest_code[len];
            
            smallest_code[len]++;
            assert(smallest_code[len] < (1 << len));
        }
    }
    
    free(bl_count);
    free(smallest_code);
    
    return unpacked_dict;
}

uint8_t *
allocate_pixels(
    uint32_t height,
    uint32_t width,
    uint32_t bytes_per_pixel)
{
    uint8_t * return_value = malloc(
        height * width * bytes_per_pixel);
    
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
    
    printf("Analyzing file: %s\n", argv[1]);
    
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
    
    uint8_t * pixels = 0;
    
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
            "IHDR",
            4))
        {
            assert(chunk_header->length == 13);
            
            bool32_t supported = true;
            
            IHDRBody * ihdr_body = consume_struct(
                /* type: */ IHDRBody,
                /* entire_file: */ entire_file);
            
            flip_endian(&ihdr_body->width);
            flip_endian(&ihdr_body->height);
           
            // (below) These PNG setting assert the RGBA color
            // space. There are many other formats (greyscale
            // illustrations etc.) found in .png files that we
            // are not supporting for now. 
            if (ihdr_body->bit_depth != 8
                || ihdr_body->color_type != 6)
            {
                printf("unsupported PNG type");
                assert(1 == 2);
                return 1;
            }
            
            // uint32_t width;
            // uint32_t height;
            // uint8_t  bit_depth;
            // uint8_t  color_type;
            // uint8_t  compression_method;
            // uint8_t  filter_method;
            // uint8_t  interlace_method;
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
            printf(
                "\tinterlace_method: %u\n",
                ihdr_body->interlace_method);

            if (supported) {
                // pixels = allocate_pixels(
                //     /* height: */
                //         ihdr_body->height,
                //     /* width: */
                //         ihdr_body->width,
                //     /* bytes_per_pixel: */
                //         4);
                pixels = malloc(
                    ihdr_body->height
                    * ihdr_body->width
                    * 4);
            }
        }
        else if (are_equal_strings(
            chunk_header->type,
            "PLTE",
            4))
        {
            // handle palette header
            break;
        }
        else if (are_equal_strings(
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
            
            // to mask the rightmost 4 we need 00001111 
            //                                     8421
            //                               8+4+2+1=15
            // to isolate the leftmost 4, we need to
            // shift right by 4
            // (I guess the padded bits are 0 by default)
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
            to mask: 15 + 16 = 31
            The FCHECK value must be such that CMF and FLG,
            when viewed as a 16-bit unsigned integer stored in
            MSB order (CMF*256 + FLG), is a multiple of 31.
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
            
            uint8_t BTYPE = reverse_bit_order(consume_bits(
                /* buffer: */ entire_file,
                /* size  : */ 2), 2);
            
            char * btype_description;
            
            /*
            In all cases, the decoding algorithm for the actual
            data is as follows:
            
            do
               read block header from input stream. (did above)
               if (stored with no compression)
                  skip any remaining bits in current partially
                     processed byte
                  read LEN and NLEN (see next section)
                  copy LEN bytes of data to output
               else
                  if (compressed with dynamic Huffman codes
                     read representation of code trees (see
                        subsection below)
                  loop (until end of block code recognized)
                     decode literal/length value from input
                     stream
                    
                     if value < 256
                        copy value (literal byte) to output
                        stream
                     otherwise
                        if value = end of block (256)
                           break from loop
                        otherwise (value = 257..285)
                           decode distance from input stream
                           
                           move backwards dist bytes in the
                           output stream, and copy length bytes
                           from this position to the output
                           stream.
                  end loop
            while not last block
            */
            
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
                    
                    // TODO: copy LEN bytes of data to output
                    
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
            
            For even greater compactness, the code
            length sequences themselves are compressed
            using a Huffman code. The alphabet for code
            lengths is as follows:
        
            0 - 15: Represent code lengths of 0 - 15
            
            16: Copy the previous code length 3 - 6 times.
               The next 2 bits indicate repeat length
               (0 = 3, ... , 3 = 6)
                Example:  Codes 8, 16 (+2 bits 11),
                    16 (+2 bits 10) will expand to
                    12 code lengths of 8 (1 + 6 + 5)
            
            17: Repeat a code length of 0
                for 3 - 10 times.
               (3 bits of length)
            
            18: Repeat a code length of 0
                for 11 - 138 times
               (7 bits of length) 
            
            A code length of 0 indicates that the corresponding
            symbol in the literal/length or distance alphabet
            will not occur in the block, and should not
            participate in the Huffman code construction
            algorithm given earlier. If only one distance code
            is used, it is encoded using one bit, not zero bits;
            in this case there is a single code length of one,
            with one unused code. One distance code of zero
            bits means that there are no distance codes used at
            all (the data is all literals).
            */
                    // 5 Bits: HLIT (huffman literal)
                    // number of Literal/Length codes - 257
                    // (257 - 286)
                    uint32_t HLIT = consume_bits(
                        /* from: */ entire_file,
                        /* size: */ 5) + 257;
                    printf(
                        "\t\t\tHLIT : %u (expect 257-286)\n",
                        HLIT);
                    assert(HLIT >= 257 && HLIT <= 286);
                    
                    
                    // 5 Bits: HDIST (huffman distance?)
                    // # of Distance codes - 1
                    // (1 - 32)
                    uint32_t HDIST = consume_bits(
                        /* from: */ entire_file,
                        /* size: */ 5) + 1;
                    assert(HDIST >= 1 && HDIST <= 32);
                    printf(
                        "\t\t\tHDIST: %u (expect 1-32)\n",
                        HDIST);
                    
                    // 4 Bits: HCLEN (huffman code length)
                    // # of Code Length codes - 4
                    // (4 - 19)
                    uint32_t HCLEN = consume_bits(
                        /* from: */ entire_file,
                        /* size: */ 4);
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
                    for (uint32_t i = 0; i < HCLEN; i++) {
                        assert(swizzle[i] <
                            NUM_UNIQUE_CODELENGTHS);
                        HCLEN_table[swizzle[i]] =
                            reverse_bit_order(consume_bits(
                                /* from: */ entire_file,
                                /* size: */ 3), 3);
                        printf(
                            "\t\t\ti:%u -> %s[%u]: %u\n",
                            i,
                            "HCLEN_table",
                            swizzle[i],
                            HCLEN_table[swizzle[i]]);
                    }
                    
                    /*
                    We now have some values in HCLEN_table,
                    but these are themselves 'compressed'
                    and need to be unpacked.
                    */ 
                    printf("\t\t\tUnpack code lengths table:\n");
                    
                    HuffmanPair * codelengths_huffman =
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
                        printf(
                            "\t\t%s[%u] key:%u %s:%u val:%u\n",
                            "codelengths_huffman",
                            i,
                            codelengths_huffman[i].key,
                            "code length",
                            codelengths_huffman[i].code_length,
                            codelengths_huffman[i].value);
                        assert(
                          codelengths_huffman[i].value >= 0);
                        assert(
                          codelengths_huffman[i].value < 19);
                    }
                    
                    /*
                    Now we have an unpacked table with code
                    lengths from 0 to 18. Each code length
                    implies a different action, see below.
                    */
                    
                    uint32_t len_i = 0;
                    uint32_t output_size = HLIT + HDIST;
                    
                    uint32_t * litlendist_table = malloc(
                        sizeof(uint32_t) * output_size);
                   
                    uint32_t previous_len = 0;
                    
                    while (len_i < output_size) {
                        uint32_t encoded_len = huffman_decode(
                            /* dict: */
                                codelengths_huffman,
                            /* dictsize: */
                                NUM_UNIQUE_CODELENGTHS,
                            /* raw data: */
                                entire_file);
                        
                        if (encoded_len <= 15) {
                            printf("handle encoded_len < 16\n");
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
                            printf("handle encoded_len 16\n");
                            uint32_t extra_bits_repeat =
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 2);
                            uint32_t repeats =
                                extra_bits_repeat + 3;
                            printf(
                                "%u repeats of previous: %u\n",
                                repeats,
                                previous_len);
                            assert(repeats >= 3);
                            assert(repeats < 6);
                            
                            for (
                                int i = 0;
                                0 < repeats;
                                i++)
                            {
                                litlendist_table[len_i] =
                                    previous_len;
                                len_i++;
                            }
                        } else if (encoded_len == 17) {
                        /*
                        !COPIED FROM SPECIFICATION!
                        17: Repeat a code length of 0 for 3 - 10
                            times.
                            3 extra bits for length
                        */
                            printf("handle encoded_len 17\n");
                            uint32_t extra_bits_repeat =
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 3);
                            uint32_t repeats =
                                extra_bits_repeat + 3;
                            printf("repeats: %u\n", repeats);
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
                            printf("repeats: %u\n", repeats);
                            assert(repeats >= 11);
                            assert(repeats < 139);
                            
                            for (
                                int i = 0;
                                0 < repeats;
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
                        
                        previous_len = encoded_len;
                    }
                    
                    printf("\t\t\tfinished reading len_count");
                    assert(len_i == output_size);
                    for (int i = 0; i < 512; i++) {
                        printf(
                            "\t\t\tlitlendist_table[%u]: %u\n",
                            i,
                            litlendist_table[i]);
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
            
            break;
            
            // skip data
            uint32_t skip = chunk_header->length;
            printf(
                "\tskip unhandled lowercase noncriticl chunk\n");
            entire_file->data += skip;
            entire_file->size_left -= skip;
        }
        else if (are_equal_strings(
            chunk_header->type,
            "IEND",
            4))
        {
            // handle image end header
            break;
        }
        else if ((char)chunk_header->type[0] > 'Z')
        {
            uint32_t skip = chunk_header->length;
            printf(
                "\tskip unhandled lowercase noncriticl chunk\n");
            
            entire_file->data += skip;
            
            assert(skip <= entire_file->size_left); 
            entire_file->size_left -= skip;
        } else {
            printf(
                "ERROR: unhandled critical chunk header: %s\n",
                chunk_header->type);
            return 1;
        }
        
        // ignore a 4-byte CRC checksum
        assert(entire_file->size_left >= 4);
        entire_file->data += 4;
        entire_file->size_left -= 4;
    }
    
    return 0;
}

