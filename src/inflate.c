#include "inflate.h"

#ifndef NULL
#define NULL 0
#endif

#define NUM_UNIQUE_CODELENGTHS 19
// #define HUFFMAN_HASHMAP_SIZE 2048 // 2^11
// #define HUFFMAN_HASHMAP_SIZE 4096 // 2^12
#define HUFFMAN_HASHMAP_SIZE 8192 // 2^13
#define HUFFMAN_HASHMAP_LINKEDLIST_SIZE 10

static void align_memory(
    uint8_t ** memory_store,
    uint64_t * memory_store_size_remaining)
{
    while ((uintptr_t)(void *)*memory_store % 16 != 0) {
        *memory_store += 1;
        *memory_store_size_remaining -= 1;
    }
    
    #ifndef INFLATE_IGNORE_ASSERTS
    assert((uintptr_t)(void *)*memory_store % 16 == 0);
    #endif
}

/*
Since we need to consume partial bytes (and leave remaining bits in a buffer),
we're using this data structure.
*/
typedef struct DataStream {
    uint8_t * data;
    
    uint32_t bits_left;
    uint8_t bit_buffer;
    
    uint64_t size_left;
} DataStream;

/*
'Huffman encoding' is a famous compression algorithm
*/
typedef struct HuffmanEntry {
    uint32_t key;
    uint32_t code_length;
    uint32_t value;
    uint32_t used;
} HuffmanEntry;

/*
We'll store our huffman codes in an improvised hashmap
*/
typedef struct HashedHuffmanEntry {
   uint32_t key;
   uint32_t code_length;
   uint32_t value;
} HashedHuffmanEntry;

typedef struct HashedHuffmanLinkedList {
   HashedHuffmanEntry entries[HUFFMAN_HASHMAP_LINKEDLIST_SIZE];
   uint32_t size;
} HashedHuffmanLinkedList;

typedef struct HashedHuffman {
    HashedHuffmanLinkedList linked_lists[HUFFMAN_HASHMAP_SIZE];
    uint32_t min_code_length;
    uint32_t max_code_length;
} HashedHuffman;

inline static uint32_t mask_rightmost_bits(
    const uint32_t input,
    const uint32_t bits_to_mask)
{
    return input & ((1 << bits_to_mask) - 1);
}

inline static uint32_t mask_leftmost_bits(
    const uint32_t input,
    const uint32_t bits_to_mask)
{
    uint32_t return_value = input;
    
    uint32_t rightmost_mask = (1 << bits_to_mask) - 1;
    uint32_t leftmost_mask = rightmost_mask << (32 - bits_to_mask);
    
    return_value = return_value & leftmost_mask;
    return_value = return_value >> (32 - bits_to_mask);
    
    return return_value;
}

/*
Annoyingly, many values in DEFLATE/INFLATE compression
are stored in reversed order. I don't understand the reason
for this but I'm sure there must be one. Anyway, we will need
to do a lot of bit reversing, e.g.

If we have the number 10 (0000 1010)
then reverse_bit_order(10, 4) = 3 (0000 0101)
*/
static uint32_t reverse_bit_order(
    const uint32_t original,
    const unsigned int bit_count)
{
    // Approach explained:
    //
    // Step 1: swap byte A-B & C-D
    // bytA.... bytB.... bytC..... bytD.... 
    //  >>                           <<
    // bytC.... bytD.... bytA..... bytB....
    //
    // Step 2: swap byte A with B and C with D 
    // bytC.... bytD.... bytA..... bytB....
    //            >>       <<
    // bytD.... bytC.... bytB..... bytA....
    //
    // Step 3: swap bits 1-4 of every byte with 5-8
    // 12345678 12345678 12345678 12345678
    //   >> <<    >> <<    >> <<   >>  <<
    // 56781234 56781234 56781234 56781234
    //
    // Step 4: Swap bits 1-2 with 3-4 & bits 5-6 with 7-8
    //
    // 56781234 56781234 56781234 56781234
    // >><<>><< >><<>><< >><<>><< >><<>><<
    // 78563412 78563412 78563412 78563412
    //
    // Step 5: Swap bit 1 with 2, 3 with 4, 5 with 6, 7 with 8
    //
    // 78563412 78563412 78563412 78563412
    // ><><><>< ><><><>< ><><><>< ><><><><
    // 87654321 87654321 87654321 87654321
    //
    // Step 6: mask the rightmost bits according to bit_count
   
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(bit_count > 0);
    assert(bit_count < 33);
    #endif
    
    if (bit_count == 1) { return original; }
    
    // Step 1: swap byte A-B & C-D
    uint32_t return_value = 
        ( (original & 0xFFFF0000) >> 16)
        | (original & 0x0000FFFF) << 16;
    
    // Step 2: swap byte A with B and C with D 
    return_value =
        ( (return_value & 0xFF00FF00) >> 8)
        | (return_value & 0x00FF00FF) << 8;
    
    // Step 3: swap bits 1-4 of every byte with 5-8
    return_value =
        ( (return_value & 0xF0F0F0F0) >> 4)
        | (return_value & 0x0F0F0F0F) << 4;
    
    // Step 4: Swap bits 1-2 with 3-4 & bits 5-6 with 7-8
    return_value =
        ( (return_value & 0xCCCCCCCC) >> 2)
        | (return_value & 0x33333333) << 2;
 
    // Step 5: Swap bit 1 with 2, 3 with 4, 5 with 6, 7 with 8
    return_value =
        ( (return_value & 0xAAAAAAAA) >> 1)
        | (return_value & 0x55555555) << 1;
    
    // Step 6: mask the leftmost bits according to bit_count
    return mask_leftmost_bits(return_value, bit_count);
}

/*
Look at the top bits of our data stream, but keep them inplace
*/
inline static uint32_t peek_bits(
    DataStream * from,
    uint32_t bits_to_peek)
{
    uint32_t return_value = 0;
    uint32_t bits_in_return = 0;
    
    // read from the bit buffer first (up to 7 bits)
    uint32_t num_to_read_from_bitbuf =
        ((bits_to_peek > from->bits_left) * from->bits_left) +
        ((bits_to_peek <= from->bits_left) * bits_to_peek);
    return_value =
        mask_rightmost_bits(
            /* input: */
                from->bit_buffer,
            /* amount: */
                num_to_read_from_bitbuf);
    bits_to_peek -= num_to_read_from_bitbuf;
    bits_in_return += num_to_read_from_bitbuf;
    
    // now read from the byte buffer
    uint8_t * peek_at = from->data;
    uint32_t bytes_to_peek = bits_to_peek / 8;
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(bytes_to_peek < 4);
    #endif
    uint32_t next_bytes =
	(uint32_t)peek_at[0]
	| ((uint32_t)peek_at[1] << 8)
	| ((uint32_t)peek_at[2] << 16)
	| ((uint32_t)peek_at[3] << 24);
    next_bytes &= ((1 << (bytes_to_peek * 8)) - 1);
//     uint32_t next_bytes =
// 	*(uint32_t *)peek_at
// 	& ((1 << (bytes_to_peek * 8)) - 1);
    
    return_value +=
	(next_bytes << bits_in_return);
    
    bits_to_peek -= bytes_to_peek * 8;
    bits_in_return += bytes_to_peek * 8;
    peek_at += bytes_to_peek;
    
    // Add bits from the final byte
    // here again, if there were bits in the return
    // the new byte is 'more significant'
    // so again the return value stays the same and
    // the new partial byte gets bitshifted left
    uint32_t partial_new_byte = 
        mask_rightmost_bits(
            /* input: */ *peek_at,
            /* amount: */ bits_to_peek);
    partial_new_byte <<= bits_in_return;
    return_value |= partial_new_byte;
    
    return return_value;
}

/*
For our hashmaps, we need a hash function to index them
given the key & code length we're looking for
*/
static uint32_t compute_hash(
    uint32_t key,
    uint32_t code_length)
{
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(code_length > 0);
    assert(code_length < 19);
    #endif
    
    /*
    ABANDONED IDEA:
    code_length is between 1 and 19, so you can store it entirely
    if you can store an 18
    we can store almost everything with 4 bits (up to 15)
    1111
    8421
    if we do so, that leaves 6 bits to store the key
    
    Alternatively, we can store 3 bits of it, giving
    111
    421
    4 + 2 + 1 = 7 slots for the code length, and 7 bits for the value
    
    Note that it's better to invite conflicts for lower code lengths,
    since small code lengths have only a small possible values and therefore
    end up being able to store the entire key anyway.
    
    uint32_t code_bits = code_length < 3 ? 0 : code_length - 2;
    assert(code_bits < 16);
    uint32_t key_bits = key & ((1 << 6) - 1);
    assert(key_bits < (1 << 6));
    
    uint32_t max_hash = (15 << 6) + ((1 << 6)-1);
    assert(max_hash < HUFFMAN_HASHMAP_SIZE);
    
    uint32_t hash = ((code_bits << 6) + key_bits);
    assert(hash < HUFFMAN_HASHMAP_SIZE);
    */
    
    /*
    ABANDONED IDEA 2:
    Alternatively, we can store only 3 bits of the code length and leave
    7 bits for the key
    
    uint32_t code_bits = code_length < 4 ? 0 : ((code_length - 3) & 7);
    assert(code_bits < 8);
    uint32_t key_bits = key & ((1 << 7) - 1);
    assert(key_bits < (1 << 7));
    
    uint32_t max_hash = (7 << 7) + ((1 << 7)-1);
    assert(max_hash < HUFFMAN_HASHMAP_SIZE);
    
    uint32_t hash = ((code_bits << 7) + key_bits);
    assert(hash < HUFFMAN_HASHMAP_SIZE);
    */
    
    /*
    ABANDONED IDEA 3:
    code_length is between 1 and 19, so you can store it entirely
    if you can store an 18, so 5 bits:
    6
    18421
    
    and then it would take 19 bits for the key, so 25 in total
    that would be 33,554,432 entries for a full hashmap
    
    each entry would have 12 bytes, so that would be 400MB (!) impossible.
    */
    
    /*
    This hashing function was built by simply fiddling and
    does much better than the previous 2
    */
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(
        (key + (code_length - 1)) & (HUFFMAN_HASHMAP_SIZE - 1) <
            HUFFMAN_HASHMAP_SIZE);
    #endif
    
    return (key + (code_length - 1)) & (HUFFMAN_HASHMAP_SIZE - 1);
}

/*
Throw away the top x bits from our datastream
*/
inline static void discard_bits(
    DataStream * from,
    uint32_t amount)
{
    uint32_t bits_to_discard =
        ((from->bits_left > amount) * amount) +
        (from->bits_left <= amount) * from->bits_left;
    from->bit_buffer >>= bits_to_discard;
    from->bits_left -= bits_to_discard;
    amount -= bits_to_discard;
    
    uint32_t full_bytes_to_discard = amount / 8;
    from->data += full_bytes_to_discard;
    from->size_left -= full_bytes_to_discard;
    amount %= 8;
    
    from->bit_buffer =
        (amount > 0) * (*(uint8_t *)from->data) +
        (amount <= 0) * from->bit_buffer;
    from->data += amount > 0;
    from->size_left -= amount > 0;
    from->bits_left =
        ((amount > 0) * (8 - amount)) +
        (amount == 0) * from->bits_left;
    from->bit_buffer >>= amount;
}

static uint32_t consume_bits(
    DataStream * from,
    const uint32_t amount)
{
    #ifndef INFLATE_IGNORE_ASSERTS 
    assert(amount > 0);
    assert(amount < 33);
    #endif
    
    uint32_t bits_to_consume = amount;
    
    uint32_t return_val = peek_bits(
        from,
        bits_to_consume);
    discard_bits(
        from,
        bits_to_consume);
    
    return return_val;
}

/*
Given a datastream and a hashmap of huffman codes,
read & decompress/decode the next value

good will be set to 1 on succes, 0 on failure
*/
static uint32_t hashed_huffman_decode(
    HashedHuffman * dict,
    DataStream * datastream,
    uint32_t * good)
{
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(dict != NULL);
    assert(datastream != NULL);
    #endif
    
    uint32_t bitcount = dict->min_code_length - 1;
    uint32_t upcoming_bits = peek_bits(
        /* from: */ datastream,
        /* size: */ 24);
    uint32_t raw = 0;
    
    while (bitcount < dict->max_code_length) {
        bitcount += 1;
        
        /*
        Spec:
        "Huffman codes are packed starting with the most-
            significant bit of the code."
        
        Attention: The hash keys are already reversed,
        so we can just compare the raw values to the keys 
        */
        raw = mask_rightmost_bits(upcoming_bits, bitcount);
        
        uint32_t hash = compute_hash(
            /* key: */ raw,
            /* code_length: */ bitcount);
        
        for (uint32_t l = 0; l < dict->linked_lists[hash].size; l++) {
            #ifndef INFLATE_IGNORE_ASSERTS
            assert(l < HUFFMAN_HASHMAP_LINKEDLIST_SIZE);
            #endif
            if (
                dict->linked_lists[hash].entries[l].key == raw
                && dict->linked_lists[hash].entries[l].code_length == bitcount)
            {
                discard_bits(datastream, bitcount);
                *good = 1;
                return dict->linked_lists[hash].entries[l].value;
            }
        }
    }
    
    #ifndef INFLATE_SILENCE 
    printf(
        "failed to find raw :%u for codelen: %u in dict\n",
        raw,
        bitcount);
    #endif
    *good = 0;
    
    return 0;
}

static void construct_hashed_huffman(
    HashedHuffman * to_construct)
{
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(to_construct != NULL);
    #endif
    
    to_construct->min_code_length = 9999;
    to_construct->max_code_length = 1;
    
    // init linked list sizes to 0
    for (uint32_t i = 0; i < HUFFMAN_HASHMAP_SIZE; i++)
    {
        to_construct->linked_lists[i].size = 0;
    }
}

/*
Convert an array of huffman codes to a hashmap of huffman codes

working_memory_remaining is both an input & output variable
*/
static void huffman_to_hashmap(
    HuffmanEntry * huffman_input,
    const uint32_t huffman_input_size,
    HashedHuffman * recipient)
{
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(huffman_input  != NULL);
    assert(huffman_input_size > 0);
    #endif
    
    construct_hashed_huffman(recipient);
    
    for (uint32_t i = 0; i < huffman_input_size; i++)
    {
        if (!huffman_input[i].used) {
            continue;
        }
        
        // we'll store the reversed key, so that we don't
        // have to reverse on each lookup
        uint32_t reversed_key = reverse_bit_order(
            /* raw: */ huffman_input[i].key,
            /* size: */ huffman_input[i].code_length);
        
        uint32_t hash = compute_hash(
            /* key: */ reversed_key,
            /* code_length: */ huffman_input[i].code_length);
        
        #ifndef INFLATE_IGNORE_ASSERTS
        assert(hash <= HUFFMAN_HASHMAP_SIZE);
        assert(hash >= 0);
        #endif
        
        if (
            huffman_input[i].code_length < recipient->min_code_length)
        {
            recipient->min_code_length = huffman_input[i].code_length;
        } else if (
            huffman_input[i].code_length > recipient->max_code_length)
        {
            recipient->max_code_length = huffman_input[i].code_length;
            #ifndef INFLATE_IGNORE_ASSERTS
            assert(recipient->max_code_length < 30);
            #endif
        }
        
        uint32_t list_size = recipient->linked_lists[hash].size;
        recipient->linked_lists[hash].entries[list_size].key = reversed_key;
        recipient->linked_lists[hash].entries[list_size].code_length =
            huffman_input[i].code_length;
        recipient->linked_lists[hash].entries[list_size].value =
            huffman_input[i].value;
        recipient->linked_lists[hash].size += 1;
        #ifndef INFLATE_IGNORE_ASSERTS
        assert(recipient->linked_lists[hash].size <
            HUFFMAN_HASHMAP_LINKEDLIST_SIZE);
        #endif
    }
    
    #ifndef INFLATE_IGNORE_ASSERTS 
    assert(
       recipient->min_code_length <=
       recipient->max_code_length);
    #endif
}

/*
Given an array of code lengths, unpack it to
an array of huffman codes

good will be set to 1 on success, 0 on failure
*/
static void unpack_huffman(
    uint32_t * array,
    const uint32_t array_and_recipient_size,
    HuffmanEntry * recipient,
    uint32_t * good)
{
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(array != NULL);
    assert(array_and_recipient_size > 0);
    assert(recipient != NULL);
    #endif
    
    // initialize dict
    for (uint32_t i = 0; i < array_and_recipient_size; i++) {
        recipient[i].value = i;
        recipient[i].code_length = array[i];
        recipient[i].key = 1234543;
        recipient[i].used = 0;
    }
    
    // 1) Count the number of codes for each code length.  Let
    // bl_count[N] be the number of codes of length N, N >= 1.
    uint32_t bl_count[array_and_recipient_size];
    unsigned int min_code_length = 123454321;
    unsigned int max_code_length = 0;
    for (uint32_t i = 0; i < array_and_recipient_size; i++) {
        bl_count[i] = 0;
    }
    
    for (uint32_t i = 0; i < array_and_recipient_size; i++) {
        
        uint32_t bl_count_i = array[i];
        
        if (bl_count_i >= array_and_recipient_size) {
            *good = 0;
            return;
        }
        
        if (bl_count[bl_count_i] == 0) {
        }
        if (bl_count_i > max_code_length) {
            max_code_length = bl_count_i;
        }
        if (bl_count_i < min_code_length && bl_count_i > 0) {
            min_code_length = bl_count_i;
        }
        bl_count[bl_count_i] += 1;
    }
    
    // Spec: 
    // 2) "Find the numerical value of the smallest code for each
    //    code length:"
    uint32_t smallest_code[array_and_recipient_size];
    
    /*
        this code is yanked straight from the spec
        code = 0;
        bl_count[0] = 0;
        for (bits = 1; bits <= MAX_BITS; bits++) {
            code = (code + bl_count[bits-1]) << 1;
            next_code[bits] = code;
        }
    */
    unsigned int code = 0;
    bl_count[0] = 0;
    
    #ifndef INFLATE_IGNORE_ASSERTS 
    assert(max_code_length < array_and_recipient_size);
    #endif
    
    for (
        uint32_t bits = 1;
        bits <= max_code_length; 
        bits++)
    {
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
            uint32_t actually_used = 0;
            for (uint32_t i = 0; i < array_and_recipient_size; i++) {
                if (recipient[i].code_length == bits) {
                    actually_used = 1;
                }
            }
            
            if (actually_used) {
                #ifndef INFLATE_SILENCE 
                printf(
                    "ERROR: smallest_code[%u] was %u%s",
                    bits,
                    smallest_code[bits],
                    " - value can't fit in that few bits!\n");
                #endif
                
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(0);
                #endif
            }
        }
    }
    
    // Spec: 
    // 3) "Assign numerical values to all codes, using
    //    consecutive values for all codes of the same length
    //    with the base values determined at step 2. Codes that
    //    are never used (which have a bit length of zero) must
    //    not be assigned a value."
    for (uint32_t n = 0; n < array_and_recipient_size; n++) {
        
        uint32_t len = recipient[n].code_length;
        
        if (len >= min_code_length) {
            recipient[n].key = smallest_code[len];
            recipient[n].used = 1;
            
            smallest_code[len]++;
        }
    }
    
    #ifndef INFLATE_IGNORE_ASSERTS
    uint32_t found_used = 0;
    for (uint32_t i = 0; i < array_and_recipient_size; i++) {
        
        if (recipient[i].used) {
            found_used = 1;
            break;
        }
    }
    assert(found_used);
    #endif
    
    *good = 1;
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
    {269, 2, 19},
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
    {12, 5, 65}, // we want 88? 88 - 65 = 23. 12 + 10111
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

// This is the 'API method' provided by this file
// Given some data that was compressed using the DEFLATE
// or 'zlib' algorithm, you can 'INFLATE' it back 
// to the original
// returns 1 when failed, 0 when succesful
void inflate(
    uint8_t const * recipient,
    const uint64_t recipient_size,
    uint64_t * final_recipient_size,
    uint8_t const * temp_working_memory,
    const uint64_t temp_working_memory_size,
    uint8_t const * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_good)
{
    if (recipient == NULL) {
        #ifndef INFLATE_SILENCE
        printf(
            "inflate() ERROR: was passed a NULL recipient, cant write data\n");
        #endif
        *out_good = 0;
        return;
    }
    
    if (final_recipient_size == NULL) {
        #ifndef INFLATE_SILENCE
        printf(
            "inflate() ERROR: was passed a NULL final_recipient_size, cant "
            " store the size of the uncompressed data\n");
        #endif
        *out_good = 0;
        return;
    }
    
    if (compressed_input == NULL) {
        #ifndef INFLATE_SILENCE
        printf(
            "inflate() ERROR: was passed a NULL compressed_input, cant read "
            " data to uncompress\n");
        #endif
        *out_good = 0;
        return;
    }
    
    if (recipient_size < compressed_input_size) {
        #ifndef INFLATE_SILENCE
        printf(
            "inflate() ERROR: recipient size was smaller than input, no "
            "room to uncompress\n");
        #endif
        *out_good = 0;
        return;
    }
    
    if (compressed_input_size < 5) {
        #ifndef INFLATE_SILENCE
        printf(
            "inflate() ERROR: compressed_input_size was only %llu\n",
            compressed_input_size);
        #endif
        *out_good = 0;
        return;
    }
    
    #ifndef INFLATE_SILENCE
    printf(
        "\t\tstart INFLATE expecting %llu bytes of compressed data\n",
        compressed_input_size);
    #endif
    uint8_t * recipient_at = (uint8_t *)recipient;
    *final_recipient_size = 0;
    
    DataStream data_stream;
    data_stream.data = (uint8_t *)compressed_input;
    data_stream.size_left = compressed_input_size;
    data_stream.bits_left = 0;
    data_stream.bit_buffer = 0;
    
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(data_stream.data != NULL); 
    assert(data_stream.size_left >= compressed_input_size);
    assert(data_stream.bits_left == 0);
    #endif
    
    int read_more_deflate_blocks = 1;
    
    while (read_more_deflate_blocks) {
        // reset working memory and overwrite previous hash tables
        uint8_t * working_memory_at = (uint8_t *)temp_working_memory;
        uint64_t working_memory_remaining = temp_working_memory_size;
        
        #ifndef INFLATE_SILENCE
        printf("\t\treading new DEFLATE block...\n");
        #endif
        
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
        uint32_t BFINAL = consume_bits(
            /* buffer: */ &data_stream,
            /* size  : */ 1);
        #ifndef INFLATE_IGNORE_ASSERTS
        assert(BFINAL < 2);
        #endif
        
        #ifndef INFLATE_SILENCE
        printf(
            "\t\t\tBFINAL (flag for final block): %u\n",
            BFINAL);
        #endif
        if (BFINAL) { read_more_deflate_blocks = 0; }
        
        uint32_t BTYPE = consume_bits(
            /* buffer: */ &data_stream,
            /* size  : */ 2);
        
        if (BTYPE == 0) {
            #ifndef INFLATE_SILENCE
            printf("\t\t\tBTYPE 0 - No compression\n");
            #endif
            
            // spec says to ditch remaining bits
            if (data_stream.bits_left > 0) {
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tditching a byte with %u%s\n",
                    data_stream.bits_left,
                    " bits left...");
                #endif
                
                discard_bits(
                    /* from: */ &data_stream,
                    /* amount: */ data_stream.bits_left);
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(data_stream.bits_left == 0);
                #endif
            }
            
            uint16_t LEN =
                (uint16_t)consume_bits(&data_stream, 16);
            #ifndef INFLATE_SILENCE
            printf(
                "\t\t\tuncompr. block has LEN: %u bytes\n",
                LEN);
            #endif
            
            uint16_t NLEN =
                (uint16_t)consume_bits(&data_stream, 16);
            if ((uint16_t)LEN != (uint16_t)~NLEN) {
                #ifndef INFLATE_SILENCE
                printf(
                    "inflate() ERROR: LEN didn't match NLEN\n");
                #endif
                *out_good = 0;
                return;
            }
            
            for (int _ = 0; _ < LEN; _++) {
                *recipient_at = *(uint8_t *)data_stream.data;
                recipient_at++;
                *final_recipient_size += 1;
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(*final_recipient_size <= recipient_size);
                #endif
                
                if ((uint64_t)(recipient_at - recipient)
                    >= recipient_size)
                {
                    #ifndef INFLATE_SILENCE
                    printf(
                        "ERROR - recipient overflow! recipient_at: %p - "
                        "recipient: %p = %llu, but recipient_size only: %llu\n",
                        recipient_at,
                        recipient,
                        (uint64_t)(recipient_at - recipient),
                        recipient_size);
                    *out_good = 0;
                    return;
                    #endif
                }
                
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(
                    (recipient_at - recipient) <= (uint32_t)recipient_size);
                #endif
                data_stream.data++;
                data_stream.size_left--;
            }
        } else if (BTYPE > 2) {
            #ifndef INFLATE_SILENCE
            printf(
                "\t\t\tERROR - unexpected deflate BTYPE %u\n",
                BTYPE);
            #endif
            #ifndef INFLATE_IGNORE_ASSERTS
            assert(0);
            #endif
        } else {
            #ifndef INFLATE_IGNORE_ASSERTS
            assert(BTYPE >= 1 && BTYPE <= 2);
            #endif
            
            // used in both dynamic & fixed huffman encoded files
            HuffmanEntry * literal_length_huffman = NULL;
            HashedHuffman * hashed_litlen_huffman = NULL;
            
            // only used in dynamic, keep NULL for fixed 
            HuffmanEntry * distance_huffman = NULL;
            HashedHuffman * hashed_dist_huffman = NULL;
            
            // will be overwritten in dynamic
            // leave 288 for fixed
            uint32_t HLIT = 288; 
            // only used in dynamic, keep 0 for fixed
            uint32_t HDIST = 0;
            
            if (BTYPE == 1) {
                #ifndef INFLATE_SILENCE
                printf("\t\t\tBTYPE 1 - Fixed Huffman\n");
                #endif
                
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(distance_huffman == NULL);
                #endif
                
                /*
                The Huffman codes for the two alphabets
                are fixed, and are not represented explicitly
                in the data.
                
                The Huffman code lengths for the literal/length
                alphabet are:
                
                Lit Value    Bits   Codes
                ---------    ----   -----
                0   - 143     8     00110000 through 10111111
                144 - 255     9     110010000 through 111111111
                256 - 279     7     0000000 through 0010111
                280 - 287     8     11000000 through 11000111
                */
                
                uint32_t fixed_hclen_table[288] = {};
                for (int i = 0; i < 144; i++) {
                    fixed_hclen_table[i] = 8;
                }
                for (int i = 144; i < 256; i++) {
                    fixed_hclen_table[i] = 9;
                }
                for (int i = 256; i < 280; i++) {
                    fixed_hclen_table[i] = 7;
                }
                for (int i = 280; i < 288; i++) {
                    fixed_hclen_table[i] = 8;
                }
                
                uint32_t ll_good = 0;
                if (working_memory_remaining < sizeof(HuffmanEntry) * 288) {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                
                align_memory(&working_memory_at, &working_memory_remaining);
                literal_length_huffman = (HuffmanEntry *)working_memory_at;
                working_memory_at += sizeof(HuffmanEntry) * 288;
                working_memory_remaining -= sizeof(HuffmanEntry) * 288;
                unpack_huffman(
                    /* array:     : */
                        fixed_hclen_table,
                    /* array_and_recipient_size : */
                        288,
                    /* recipient: */
                        literal_length_huffman,
                    /* good:      : */
                        &ll_good);
                
                if (!ll_good) {
                    #ifndef INFLATE_SILENCE
                    printf(
                        "INFLATE failed, "
                        "bad literal length huffman unpack\n");
                    #endif
                    *out_good = 0;
                    return;
                }
               
                #ifndef INFLATE_IGNORE_ASSERTS 
                assert(literal_length_huffman[0].value == 0);
                assert(
                    literal_length_huffman[0].code_length == 8);
                assert(literal_length_huffman[0].key == 48);
                assert(literal_length_huffman[143].value == 143);
                assert(
                    literal_length_huffman[143].code_length == 8);
                assert(literal_length_huffman[143].key == 191);
                assert(literal_length_huffman[144].value == 144);
                assert(
                    literal_length_huffman[144].code_length == 9);
                assert(literal_length_huffman[144].key == 400);
                assert(literal_length_huffman[255].value == 255);
                assert(
                    literal_length_huffman[255].code_length == 9);
                assert(literal_length_huffman[255].key == 511);
                assert(literal_length_huffman[256].value == 256);
                assert(
                    literal_length_huffman[256].code_length == 7);
                assert(literal_length_huffman[256].key == 0);
                assert(literal_length_huffman[279].value == 279);
                assert(
                    literal_length_huffman[279].code_length == 7);
                assert(literal_length_huffman[279].key == 23);
                assert(literal_length_huffman[280].value == 280);
                assert(
                    literal_length_huffman[280].code_length == 8);
                assert(literal_length_huffman[280].key == 192);
                assert(literal_length_huffman[287].value == 287);
                assert(
                    literal_length_huffman[287].code_length == 8);
                assert(literal_length_huffman[287].key == 199);
                #endif
                
                if (working_memory_remaining < sizeof(HashedHuffman)) {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                align_memory(&working_memory_at, &working_memory_remaining);
                hashed_litlen_huffman =
                    (HashedHuffman *)working_memory_at;
                working_memory_at += sizeof(HashedHuffman);
                working_memory_remaining -= sizeof(HashedHuffman);
                huffman_to_hashmap(
                    /* huffman_input: */
                        literal_length_huffman,
                    /* huffman_input_size: */
                        HLIT,
                    /* recipient: */
                        hashed_litlen_huffman);
                
                if (hashed_litlen_huffman == NULL) {
                    *out_good = 0;
                    return;
                }
                
                #ifndef INFLATE_SILENCE 
                printf("\t\t\tcreated fixed huffman dict.\n");
                #endif
            } else {
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(BTYPE == 2);
                #endif
                
                #ifndef INFLATE_SILENCE
                printf("\t\t\tBTYPE 2 - Dynamic Huffman\n");
                printf("\t\t\tRead code trees...\n");
                #endif
                
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
                HLIT = consume_bits(
                    /* from: */ &data_stream,
                    /* size: */ 5)
                        + 257;
                
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tHLIT : %u (expect 257-286)\n",
                    HLIT);
                #endif
                
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(HLIT >= 257 && HLIT <= 286);
                #endif
                
                // 5 Bits: HDIST (huffman distance?)
                // # of Distance codes - 1
                // (1 - 32)
                HDIST = consume_bits(
                    /* from: */ &data_stream,
                    /* size: */ 5)
                        + 1;
                
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tHDIST: %u (expect 1-32)\n",
                    HDIST);
                #endif
                
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(HDIST >= 1 && HDIST <= 32);
                #endif
                
                // 4 Bits: HCLEN (huffman code length)
                // # of Code Length codes - 4
                // (4 - 19)
                uint32_t HCLEN = consume_bits(
                    /* from: */ &data_stream,
                    /* size: */ 4)
                        + 4;
                
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tHCLEN: %u (4-19 vals of 0-6)\n",
                     HCLEN);
                #endif
               
                #ifndef INFLATE_IGNORE_ASSERTS 
                assert(
                    HCLEN >= 4
                    && HCLEN <= 19);
                #endif
                
                // This is a fixed swizzle (order of elements
                // in an array) that's agreed upon in the
                // specification.
                // I never would have imagined people from 1996
                // went this far to save 36 bits of disk space
                uint32_t swizzle[] =
                    {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                     11, 4, 12, 3, 13, 2, 14, 1, 15};
                
                // read (HCLEN + 4) x 3 bits
                // these are code lengths for the code length
                // dictionary,
                // and they'll come in "swizzled" order
                
                uint32_t HCLEN_table[
                    NUM_UNIQUE_CODELENGTHS] = {};
                
                #ifndef INFLATE_SILENCE
                printf("\t\t\tReading raw code lengths\n");
                #endif
                
                for (uint32_t i = 0; i < HCLEN; i++) {
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(swizzle[i] <
                        NUM_UNIQUE_CODELENGTHS);
                    #endif
                    
                    HCLEN_table[swizzle[i]] =
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ 3);
                    
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(HCLEN_table[swizzle[i]] <= 7);
                    assert(HCLEN_table[swizzle[i]] >= 0);
                    #endif
                }
                
                /*
                We now have some values in HCLEN_table,
                but these are themselves 'compressed'
                and need to be unpacked
                */
                #ifndef INFLATE_SILENCE
                printf("\t\t\tUnpack codelengths table...\n");
                #endif
                
                if (working_memory_remaining <
                    sizeof(HuffmanEntry) * NUM_UNIQUE_CODELENGTHS)
                {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                uint32_t cl_good = 0;
                align_memory(&working_memory_at, &working_memory_remaining);
                HuffmanEntry * codelengths_huffman =
                    (HuffmanEntry *)working_memory_at;
                working_memory_at +=
                    sizeof(HuffmanEntry) * NUM_UNIQUE_CODELENGTHS;
                working_memory_remaining -=
                    sizeof(HuffmanEntry) * NUM_UNIQUE_CODELENGTHS;
                
                unpack_huffman(
                    /* array:     : */
                        HCLEN_table,
                    /* array_and_recipient_size : */
                        NUM_UNIQUE_CODELENGTHS,
                    /* recipient: */
                        codelengths_huffman,
                    /* good: */
                        &cl_good);
                
                if (!cl_good) {
                    #ifndef INFLATE_SILENCE
                    printf("INFLATE failed, bad huffman unpack\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                
                if (working_memory_remaining < sizeof(HashedHuffman))
                {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                align_memory(&working_memory_at, &working_memory_remaining);
                HashedHuffman * hashed_clen_huffman =
                    (HashedHuffman *)working_memory_at;
                working_memory_at += sizeof(HashedHuffman);
                working_memory_remaining -= sizeof(HashedHuffman);
                huffman_to_hashmap(
                    /* huffman_input: */
                        codelengths_huffman,
                    /* huffman_input_size: */
                        NUM_UNIQUE_CODELENGTHS,
                    /* recipient: */
                        hashed_clen_huffman);
                
                if (hashed_clen_huffman == NULL) {
                    *out_good = 0;
                    return;
                }
                
                for (
                    int i = 0;
                    i < NUM_UNIQUE_CODELENGTHS;
                    i++)
                {
                    if (codelengths_huffman[i].used == 1) {
                        #ifndef INFLATE_IGNORE_ASSERTS
                        assert(
                          codelengths_huffman[i].key >= 0);
                        assert(
                          codelengths_huffman[i].value >= 0);
                        assert(
                          codelengths_huffman[i].value < 19);
                        #endif
                    }
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
                
                if (working_memory_remaining
                    < sizeof(uint32_t) * two_dicts_size)
                {
                    #ifndef INFLATE_SILENCE
                    printf(
                        "ERROR - inflate() ran out of working memory, need %lu "
                        "for literal length distance table, but only have %llu "
                        "left.\n",
                        sizeof(uint32_t) * two_dicts_size,
                        working_memory_remaining); 
                    #endif
                    *out_good = 0;
                    return;
                }
                align_memory(&working_memory_at, &working_memory_remaining);
                uint32_t * litlendist_table = (uint32_t *)working_memory_at;
                working_memory_at += sizeof(uint32_t) * two_dicts_size;
                working_memory_remaining -= sizeof(uint32_t) * two_dicts_size;
                
                while (len_i < two_dicts_size) {
                    uint32_t clen_good = 0;
                    uint32_t encoded_len =
                        hashed_huffman_decode(
                            /* dict: */
                                hashed_clen_huffman,
                            /* raw data: */
                                &data_stream,
                            /* good: */
                                &clen_good);
                    
                    if (!clen_good) {
                        #ifndef INFLATE_SILENCE
                        printf(
                            "inflate() failed, bad huffman decode\n");
                        #endif
                        *out_good = 0;
                        return;
                    }
                    
                    if (encoded_len <= 15) {
                        litlendist_table[len_i] =
                            encoded_len;
                        len_i++;
                    } else if (encoded_len == 16) {
                        /*
                        16: Copy previous code length 3-6 times.
                            2 extra bits for repeat length
                            (0 = 3, ... , 3 = 6)
                        */
                        uint32_t extra_bits_repeat =
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ 2);
                        uint32_t repeats =
                            extra_bits_repeat + 3;
                       
                        #ifndef INFLATE_IGNORE_ASSERTS 
                        assert(repeats >= 3);
                        assert(repeats <= 6);
                        assert(len_i > 0);
                        #endif
                        
                        for (
                            uint32_t i = 0;
                            i < repeats;
                            i++)
                        {
                            litlendist_table[len_i] =
                                litlendist_table[len_i - 1];
                            len_i++;
                        }
                    } else if (encoded_len == 17) {
                        /*
                        17: Repeat a code length of 0 for 3 - 10
                            times.
                            3 extra bits for length
                        */
                        uint32_t extra_bits_repeat =
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ 3);
                        uint32_t repeats =
                            extra_bits_repeat + 3;
                       
                        #ifndef INFLATE_IGNORE_ASSERTS 
                        assert(repeats >= 3);
                        assert(repeats < 11);
                        #endif
                        
                        for (
                            uint32_t i = 0;
                            i < repeats;
                            i++)
                        {
                            litlendist_table[len_i] = 0;
                            len_i++;
                        }
                        
                    } else if (encoded_len == 18) {
                        /*
                        18: Repeat a code length of 0 for
                            11 - 138 times
                            7 extra bits for length
                        */
                        uint32_t extra_bits_repeat =
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ 7);
                        uint32_t repeats =
                            extra_bits_repeat + 11;
                        
                        #ifndef INFLATE_IGNORE_ASSERTS
                        assert(repeats >= 11);
                        assert(repeats < 139);
                        #endif
                        
                        for (
                            uint32_t i = 0;
                            i < repeats;
                            i++)
                        {
                            litlendist_table[len_i] = 0;
                            len_i++;
                        }
                    } else {
                        #ifndef INFLATE_SILENCE
                        printf(
                            "ERROR : encoded_len %u\n",
                            encoded_len);
                        #endif
                        #ifndef INFLATE_IGNORE_ASSERTS
                        assert(0);
                        #endif
                    }
                }
                
                #ifndef INFLATE_SILENCE
                printf("\t\t\tfinished reading two dicts\n");
                #endif
                
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(len_i == two_dicts_size);
                #endif
                
                if (working_memory_remaining < sizeof(HuffmanEntry) * HLIT)
                {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                uint32_t litlen_good = 0;
                align_memory(&working_memory_at, &working_memory_remaining);
                literal_length_huffman = (HuffmanEntry *)working_memory_at;
                working_memory_at += sizeof(HuffmanEntry) * HLIT;
                working_memory_remaining -= sizeof(HuffmanEntry) * HLIT;
                unpack_huffman(
                    /* array:     : */
                        litlendist_table,
                    /* array_size : */
                        HLIT,
                    /* recipient: */
                        literal_length_huffman,
                    /* good       : */
                        &litlen_good);
                if (!litlen_good) {
                    #ifndef INFLATE_SILENCE
                    printf("INFLATE failed, bad huffman unpack\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                
                if (working_memory_remaining < sizeof(HashedHuffman))
                {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                align_memory(&working_memory_at, &working_memory_remaining);
                hashed_litlen_huffman = (HashedHuffman *)working_memory_at;
                working_memory_at += sizeof(HashedHuffman);
                working_memory_remaining -= sizeof(HashedHuffman); 
                huffman_to_hashmap(
                    /* huffman_input: */
                        literal_length_huffman,
                    /* huffman_input_size: */
                        HLIT,
                    /* recipient: */
                        hashed_litlen_huffman);
                if (hashed_litlen_huffman == NULL) {
                    *out_good = 0;
                    return;
                }
                
                #ifndef INFLATE_SILENCE 
                printf(
                    "\t\t\tunpacked lit/len dict\n");
                #endif
                
                for (uint32_t i = 0; i < HLIT; i++) {
                    if (literal_length_huffman[i].used == 1) {
                        #ifndef INFLATE_IGNORE_ASSERTS
                        assert(
                            literal_length_huffman[i].value
                                == i);
                        assert(
                            literal_length_huffman[i].key
                                < 99999);
                        #endif
                    }
                }
                
                uint32_t dist_good = 0;
                if (working_memory_remaining < sizeof(HashedHuffman)) {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                align_memory(&working_memory_at, &working_memory_remaining);
                distance_huffman = (HuffmanEntry *)working_memory_at;
                working_memory_at += sizeof(HashedHuffman);
                working_memory_remaining -= sizeof(HashedHuffman);
                unpack_huffman(
                    /* array:     : */
                        litlendist_table + HLIT,
                    /* array_size : */
                        HDIST,
                    /* recipient: */
                        distance_huffman,
                    /* good       : */
                        &dist_good);
                if (!dist_good) {
                    #ifndef INFLATE_SILENCE
                    printf("INFLATE failed, bad huffman unpack\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                
                if (working_memory_remaining < sizeof(HashedHuffman)) {
                    #ifndef INFLATE_SILENCE
                    printf("inflate() failing - ran out of working memory\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                align_memory(&working_memory_at, &working_memory_remaining);
                hashed_dist_huffman = (HashedHuffman *)working_memory_at;
                working_memory_at += sizeof(HashedHuffman);
                working_memory_remaining -= sizeof(HashedHuffman);
                huffman_to_hashmap(
                    /* huffman_input: */
                        distance_huffman,
                    /* huffman_input_size: */
                        HDIST,
                    /* recipient: */
                        hashed_dist_huffman);
                if (hashed_dist_huffman == NULL) {
                    *out_good = 0;
                    return;
                }
                
                #ifndef INFLATE_SILENCE
                printf("\t\t\tunpacked distance dictionary\n");
                #endif
                
                for (uint32_t i = 0; i < HDIST; i++) {
                    if (distance_huffman[i].used == 1) {
                        #ifndef INFLATE_IGNORE_ASSERTS
                        assert(distance_huffman[i].value == i);
                        assert(distance_huffman[i].key < 99999);
                        #endif
                    }
                }
            }
            
            // the remaining part of the algorithm is mostly the
            // same whether we're using dynamic huffman tables
            // or fixed huffman tables - we just use a different
            // literal-length-dictionary.
            // The only other difference is in the case of fixed
            // huffman, when we need a distance value, it's
            // consumed directly from the stream, whereas for
            // dynamic huffman  it has to be decoded using the
            // distance dictionary we prepared
            if (BTYPE == 2) {
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(distance_huffman != NULL);
                assert(HDIST > 0);
                assert(HLIT > 0);
                assert(HLIT < 300);
                #endif
            } else if (BTYPE == 1) {
                #ifndef INFLATE_IGNORE_ASSERTS
                assert(distance_huffman == NULL);
                assert(HDIST == 0);
                assert(HLIT == 288);
                #endif
            }
            
            #ifndef INFLATE_IGNORE_ASSERTS
            assert(hashed_litlen_huffman != NULL);
            #endif
            
            while (1) {
                // we should normally break from this loop
                // because we hit the magical value 256,
                // not because of running out of bytes
                
                if ((uint64_t)(data_stream.data - compressed_input)
                    >= compressed_input_size)
                {
                    #ifndef INFLATE_SILENCE
                    printf(
                        "\t\tWarning: breaking from DEFLATE preemptively "
                        "because %li bytes were read - didn't find end of "
                        "litlen (256)\n",
                        data_stream.data - compressed_input);
                    printf(
                        "\t\tcompressed_input_size was: %llu\n",
                        compressed_input_size);
                    #endif
                    read_more_deflate_blocks = 0;
                    break;
                }
                
                uint32_t litlen_good = 0; 
                uint32_t litlenvalue = hashed_huffman_decode(
                    /* dict: */
                        hashed_litlen_huffman,
                    /* raw data: */
                        &data_stream,
                    /* good: */
                        &litlen_good);
                if (!litlen_good) {
                    #ifndef INFLATE_SILENCE
                    printf(
                        "inflate() failed, bad huffman decode\n");
                    #endif
                    *out_good = 0;
                    return;
                }
                
                if (litlenvalue < 256) {
                    // literal value, not a length
                    *recipient_at =
                        (uint8_t)(litlenvalue & 255);
                    recipient_at++;
                    *final_recipient_size += 1;
                    
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(*final_recipient_size < recipient_size);
                    assert(
                        (uint64_t)(recipient_at - recipient)
                            <= recipient_size);
                    #endif
                    
                } else if (litlenvalue > 256) {
                    // length, (therefore also need distance)
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(litlenvalue < 286);
                    #endif
                    uint32_t i = litlenvalue - 257;
                    
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(
                        length_extra_bits_table[i].value
                            == litlenvalue);
                    #endif
                    
                    uint32_t extra_bits =
                        length_extra_bits_table[i]
                            .num_extra_bits;
                    uint32_t base_length =
                        length_extra_bits_table[i]
                            .base_decoded;
                    uint32_t extra_length =
                        extra_bits > 0 ?
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ extra_bits)
                        : 0;
                    uint32_t total_length =
                        base_length + extra_length;
                    
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(
                        total_length >=
                        length_extra_bits_table[i]
                            .base_decoded);
                    #endif
                    
                    uint32_t distvalue;
                    
                    if (distance_huffman == NULL) {
                        distvalue = reverse_bit_order(
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ 5),
                            5);
                    } else {
                        uint32_t hashed_dist_good = 0;
                        distvalue = hashed_huffman_decode(
                            /* dict: */
                                hashed_dist_huffman,
                            /* raw data: */
                                &data_stream,
                            /* good: */
                                &hashed_dist_good);
                        if (!hashed_dist_good) {
                            #ifndef INFLATE_SILENCE
                            printf(
                                "inflate() failed, "
                                "bad hashed dist huffman decode\n");
                            #endif
                            *out_good = 0;
                            return;
                        }
                    }
                    
                    if (distvalue > 29) {
                        #ifndef INFLATE_SILENCE
                        printf("distvalue > 29, failing...\n");
                        #endif
                        *out_good = 0;
                        return;
                    }
                    
                    if (
                        dist_extra_bits_table[distvalue].value
                            != distvalue)
                    {
                        #ifndef INFLATE_SILENCE
                        printf("extra bits table != distvalue, failing...\n");
                        #endif
                        *out_good = 0;
                        return;
                    }
                    
                    uint32_t dist_extra_bits =
                        dist_extra_bits_table[distvalue]
                            .num_extra_bits;
                    uint32_t base_dist =
                        dist_extra_bits_table[distvalue]
                            .base_decoded;
                    
                    uint32_t dist_extra_bits_decoded =
                        dist_extra_bits > 0 ?
                            consume_bits(
                                /* from: */ &data_stream,
                                /* size: */ dist_extra_bits)
                            : 0;
                    
                    uint32_t total_dist =
                        base_dist + dist_extra_bits_decoded;
                    
                    // go back dist bytes, then copy length bytes
                    if (recipient_at - total_dist < recipient) {
                        #ifndef INFLATE_SILENCE
                        printf(
                            "ERROR - can't repeat data from %u bytes before, "
                            "address is out of bounds\n",
                            total_dist);
                        #endif
                        *out_good = 0;
                        return;
                    }
                    uint8_t * back_dist_bytes =
                        recipient_at - total_dist;
                    for (uint32_t _ = 0; _ < total_length; _++) {
                        *recipient_at = *back_dist_bytes;
                        recipient_at++;
                        *final_recipient_size += 1;
                        if (recipient_at >= working_memory_at) {
                            #ifndef INFLATE_SILENCE
                            printf(
                                "ERROR - recipient overflowing into working "
                                "memory!\n");
                            *out_good = 0;
                            return;
                            #endif
                        }
                        #ifndef INFLATE_IGNORE_ASSERTS
                        assert(*final_recipient_size <= recipient_size);
                        assert(
                            (recipient_at - recipient) <
                                (uint32_t)recipient_size);
                        #endif
                        back_dist_bytes++;
                    }
                } else {
                    #ifndef INFLATE_IGNORE_ASSERTS
                    assert(litlenvalue == 256);
                    #endif
                    
                    #ifndef INFLATE_SILENCE
                    printf("\t\tend of ltln found!\n");
                    #endif
                    
                    break;
                }
            }
            
            literal_length_huffman = NULL;
            distance_huffman = NULL;
        }
    }
    
    if (data_stream.bits_left != 0) {
        #ifndef INFLATE_SILENCE
        printf(
            "\t\tpartial byte left after DEFLATE\n");
        printf(
            "\t\tdiscarding: %u bits\n",
            data_stream.bits_left);
        #endif
        
        discard_bits(
            /* from: */ &data_stream,
            /* amount: */ data_stream.bits_left);
    }
    
    uint32_t bytes_read =
        (uint32_t)(data_stream.data - compressed_input);
    #ifndef INFLATE_IGNORE_ASSERTS
    assert(bytes_read >= 0);
    #endif
    if (bytes_read != compressed_input_size) {
        #ifndef INFLATE_SILENCE
        printf(
           "Warning: expected to read %llu bytes but got %u\n",
            compressed_input_size,
            bytes_read);
        #endif
        
        #ifndef INFLATE_IGNORE_ASSERTS
        assert(compressed_input_size > bytes_read);
        #endif
        
        uint64_t skip = compressed_input_size -
            (uint64_t)bytes_read;
        #ifndef INFLATE_SILENCE
        printf("skipping ahead %llu bytes...\n", skip);
        #endif
        
        #ifndef INFLATE_IGNORE_ASSERTS
        assert(data_stream.size_left >= skip);
        #endif
        
        data_stream.data += skip;
        data_stream.size_left -= skip;
    }
    
    #ifndef INFLATE_SILENCE 
    printf("\t\tend of succesful inflate..\n");
    #endif
    
    *out_good = 1;
    return;
}
