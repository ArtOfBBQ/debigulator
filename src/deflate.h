typedef int32_t bool32_t;
#define false 0
#define true 1

#define NUM_UNIQUE_CODELENGTHS 19

typedef struct EntireFile {
    void * data;
    
    uint8_t bits_left;
    uint32_t bit_buffer;
    
    size_t size_left;
} EntireFile;

typedef struct HuffmanEntry {
    uint32_t key;
    uint32_t code_length;
    uint32_t value;
} HuffmanEntry;

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
uint32_t consume_bits(
    EntireFile * from,
    uint32_t bits_to_consume)
{
    assert(bits_to_consume > 0);
    assert(bits_to_consume < 33);
    
    uint32_t return_value = 0;
    int bits_in_return = 0;
    
    while (bits_to_consume > 0) {
        if (from->bits_left > 0) {
            return_value |=
                ((from->bit_buffer & 1) << bits_in_return);
            bits_in_return += 1;
            
            from->bit_buffer >>= 1;
            from->bits_left -= 1;
            bits_to_consume -= 1;
        } else {
            // TODO: this might be wrong endian
            // when consuming more than 8 bits
            from->bit_buffer <<= 8;
            from->bit_buffer |= *((uint8_t *)from->data);
            from->bits_left += 8;
            
            from->size_left -= 1;
            from->data += 1;
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

uint32_t huffman_decode(
    HuffmanEntry * dict,
    uint32_t dictsize,
    EntireFile * datastream)
{
    uint32_t raw = 0;
    int found_at = -1;
    int bitcount = 0;
    
    while (found_at == -1 && bitcount < 14) {
        bitcount += 1;
        
        /*
        Spec:
        "Huffman codes are packed starting with the most-
            significant bit of the code."

        That means that every time we load a new bit,
        we have to shift what we already have and add
        the new bit to the right.
        */
        raw = (raw << 1) |
            consume_bits(
                /* from: */ datastream,
                /* size: */ 1);
        
        for (int i = 0; i < dictsize; i++) {
            // TODO: don't just compare key, compare
            // the sequence of bits exactly
            if (dict[i].key == raw
                && bitcount == dict[i].code_length)
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
        assert(1 == 2);
    }
    
    assert(found_at >= 0);
    assert(found_at < dictsize);
    
    return dict[found_at].value;
};

HuffmanEntry * unpack_huffman(
    uint32_t * array,
    uint32_t array_size)
{
    HuffmanEntry * unpacked_dict = malloc(
        sizeof(HuffmanEntry) * array_size);
    
    // initialize dict
    for (int i = 0; i < array_size; i++) {
        unpacked_dict[i].value = i;
        unpacked_dict[i].code_length = array[i];
    }
    
    // this is straight from the deflate spec
    
    // 1) Count the number of codes for each code length.  Let
    // bl_count[N] be the number of codes of length N, N >= 1.
    uint32_t * bl_count = malloc(array_size);
    for (int i = 0; i < array_size; i++) {
        bl_count[i] = 0;
    }
    
    for (int i = 0; i < array_size; i++) {
        bl_count[array[i]] += 1;
    }
    
    // Spec: 
    // 2) "Find the numerical value of the smallest code for each
    //    code length:"
    uint32_t * smallest_code = malloc(600 * sizeof(uint32_t));
    uint32_t code = 0;
    
    // this code is yanked straight from the spec
    bl_count[0] = 0;
    for (uint32_t bits = 1; bits < 14; bits++) {
        code = (code + bl_count[bits-1]) << 1;
        smallest_code[bits] = code;
        
        // this algorithm is very tight
        // smallest_code[bits] may get a value too big
        // to fit inside bits, but that only happens when
        // there are no values for that bit size (and many codes
        // are taken up for smaller bit lengths). It could also
        // happen when the PNG is corrupted and the code lengths
        // are wrong (too many small code lengths)
        if (smallest_code[bits] >= (1 << bits)) {
            bool32_t actually_used = false;
            for (int i = 0; i < array_size; i++) {
                if (unpacked_dict[i].code_length == bits) {
                    actually_used = true;
                }
            }
           
            if (actually_used) {
                printf(
                    "ERROR: smallest_code[%u] was %u%s",
                    bits,
                    smallest_code[bits],
                    " - value can't fit in that few bits!\n");
                // TODO: uncomment assertion
                assert(1 == 2);
            }
        }
    }
    
    // Spec: 
    // 3) "Assign numerical values to all codes, using
    //    consecutive values for all codes of the same length
    //    with the base values determined at step 2. Codes that
    //    are never used (which have a bit length of zero) must
    //    not be assigned a value."
    for (uint32_t n = 0; n < array_size; n++) {
        uint32_t len = unpacked_dict[n].code_length;
        
        if (len != 0 && unpacked_dict[n].code_length > 0) {
            // TODO: uncomment assertion
            // or maybe they are supposed to overflow? IDK
            // assert(smallest_code[len] < (1 << len));
            
            unpacked_dict[n].key = smallest_code[len];
            
            smallest_code[len]++;
        }
    }
    
    free(bl_count);
    free(smallest_code);
    
    return unpacked_dict;
}

typedef struct ExtraBitsEntry {
    uint32_t value;
    uint32_t num_extra_bits;
    uint32_t base_decoded;
} ExtraBitsEntry;

// This table is defined in the deflate algorithm specification
// https://www.ietf.org/rfc/rfc1951.txt
static ExtraBitsEntry length_extra_bits_table[] = {
    {257, 0, 3}, // value, length_extra_bits, base_decoded
    {258, 0, 4},
    {259, 0, 5},
    {260, 0, 6},
    {261, 0, 7},
    {262, 0, 8},
    {263, 0, 9},
    {264, 0, 10},
    {265, 1, 11},
    {266, 1, 13},
    {267, 1, 15}, // index 10
    {268, 1, 17},
    {269, 1, 19},
    {270, 2, 23},
    {271, 2, 27},
    {272, 2, 31},
    {273, 3, 35},
    {274, 3, 43},
    {275, 3, 51},
    {276, 3, 59},
    {277, 4, 67}, // index 20
    {278, 4, 83},
    {279, 4, 99},
    {280, 4, 115},
    {281, 5, 131},
    {282, 5, 163},
    {283, 5, 195},
    {284, 5, 227},
    {285, 0, 258}, // index 28
};

static ExtraBitsEntry dist_extra_bits_table[] = {
    {0, 0, 1}, // value, distance_extra_bits, base_decoded
    {1, 0, 2},
    {2, 0, 3},
    {3, 0, 4},
    {4, 1, 5},
    {5, 1, 7},
    {6, 2, 9},
    {7, 2, 13},
    {8, 3, 17},
    {9, 3, 25},
    {10, 4, 33},
    {11, 4, 49},
    {12, 5, 65},
    {13, 5, 97},
    {14, 6, 129},
    {15, 6, 193},
    {16, 7, 257},
    {17, 7, 385},
    {18, 8, 513},
    {19, 8, 769},
    {20, 9, 1025},
    {21, 9, 1537},
    {22, 10, 2049},
    {23, 10, 3073},
    {24, 11, 4097},
    {25, 11, 6145},
    {26, 12, 8193},
    {27, 12, 12289},
    {28, 13, 16385},
    {29, 13, 24577},
};
