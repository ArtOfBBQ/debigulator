#include "inttypes.h"
#include "stdlib.h"

typedef int32_t bool32_t;
#define false 0 // undefined at end of file
#define true 1  // undefined at end of file
#define INFLATE_SILENCE
// #define IGNORE_ASSERTS

#define NUM_UNIQUE_CODELENGTHS 19
#define HUFFMAN_HASHMAP_SIZE 1023

#ifndef INFLATE_SILENCE
#include "stdio.h"
#endif

#ifndef IGNORE_ASSERTS
#include "assert.h"
#endif

typedef struct DataStream {
    void * data;
    
    uint8_t bits_left;
    uint8_t bit_buffer;
    
    size_t size_left;
} DataStream;

/*
'Huffman encoding' is a famous compression algorithm
*/
typedef struct HuffmanEntry {
    uint32_t key;
    uint32_t code_length;
    uint32_t value;
    bool32_t used;
} HuffmanEntry;

/*
We'll store our huffman codes in an improvised hashmap
*/
typedef struct HashedHuffmanEntry {
   uint32_t key;
   uint32_t code_length;
   uint32_t value;
   struct HashedHuffmanEntry * next_neighbor; 
} HashedHuffmanEntry;

typedef struct HashedHuffman {
    HashedHuffmanEntry * entries[HUFFMAN_HASHMAP_SIZE];
    unsigned int min_code_length;
    unsigned int max_code_length;
} HashedHuffman;

uint32_t mask_rightmost_bits(
    const uint32_t input,
    const int bits_to_mask)
{
    return input & ((1 << bits_to_mask) - 1);
}

uint32_t mask_leftmost_bits(
    const uint32_t input,
    const int bits_to_mask)
{
    uint32_t return_value = input;
    
    uint32_t rightmost_mask =
        (1 << bits_to_mask) - 1;
    uint32_t leftmost_mask =
        rightmost_mask << (32 - bits_to_mask);
    
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
uint32_t reverse_bit_order(
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
   
    #ifndef IGNORE_ASSERTS
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
uint32_t peek_bits(
    DataStream * from,
    const uint32_t amount)
{
    uint32_t return_value = 0;
    uint32_t bits_to_peek = amount;
    
    unsigned int bits_in_return = 0;
    
    if (from->bits_left > 0) {
        int num_to_read_from_buf =
            bits_to_peek > from->bits_left ?
                from->bits_left
                : bits_to_peek;
        
        return_value =
            mask_rightmost_bits(
                /* input: */
                    from->bit_buffer,
                /* amount: */
                    num_to_read_from_buf);
        
        bits_to_peek -= num_to_read_from_buf;
        bits_in_return += num_to_read_from_buf;
    }
    
    uint8_t * peek_at = from->data;
    while (bits_to_peek >= 8) {
        // the new byte will be 'more significant', so the
        // return value can stay the same but the new byte
        // 's values should be bitshifted left to be bigger
        uint32_t new_byte = *peek_at;
        return_value |= (new_byte <<= bits_in_return);
        peek_at++;
        bits_to_peek -= 8;
        bits_in_return += 8;
    }
    
    if (bits_to_peek > 0) {
       // Add bits from the final byte
       // here again, if there were bits in the return
       // the new byte is 'more significant'
       // so again the return value stays the same and
       // the new partial byte gets bitshifted left
       uint32_t partial_new_byte = 
            mask_rightmost_bits(
                /* input: */ *peek_at,
                /* amount: */ bits_to_peek);
       return_value |= (partial_new_byte <<= bits_in_return);
    }
    
    return return_value;
}

// TODO: handle error paths instead of just crashing
void crash_program() {
    #ifndef INFLATE_SILENCE
    printf("intentionally crashing program...\n");
    #endif
    uint8_t * bad_memory_access = NULL;
    uint8_t impossible = *bad_memory_access;
    return;
}

/*
For our hashmaps, we need a hash function to index them
given the key & code length we're looking for
*/
uint32_t compute_hash(
    uint32_t key,
    uint32_t code_length)
{
    return (key * code_length) & (HUFFMAN_HASHMAP_SIZE - 1);
}

/*
Throw away the top x bits from our datastream
*/
void discard_bits(
    DataStream * from,
    const unsigned int amount)
{
    unsigned int discards_left = amount;
    
    if (from->bits_left > 0) {
        int bits_to_discard =
            from->bits_left > discards_left ?
                discards_left
                : from->bits_left;
        
        from->bit_buffer >>= bits_to_discard;
        from->bits_left -= bits_to_discard;
        discards_left -= bits_to_discard;
    }
    
    while (discards_left >= 8) {
        from->data += 1;
        from->size_left -= 1;
        discards_left -= 8;
    }
    
    if (discards_left > 0) {
        from->bit_buffer = *(uint8_t *)from->data;
        from->data += 1;
        from->size_left -= 1;
        from->bits_left = (8 - discards_left);
        from->bit_buffer >>= discards_left;
    }
}

// TODO: this seems silly, let's get rid of this
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

/*
Grab data from our data stream and immediately cast it
to one of our structs or an 8-bit int
*/
#define consume_struct(type, from) (type *)consume_chunk(from, sizeof(type))
uint8_t * consume_chunk(
    DataStream * from,
    size_t size_to_consume)
{
    #ifndef IGNORE_ASSERTS
    assert(from->bits_left == 0);
    assert(from->size_left >= size_to_consume);
    #endif
    
    uint8_t * return_value = malloc(size_to_consume);
    
    copy_memory(
        /* from: */   from->data,
        /* to: */     return_value,
        /* size: */   size_to_consume);
    
    from->data += size_to_consume;
    from->size_left -= size_to_consume;
    
    return return_value;
}

uint32_t consume_bits(
    DataStream * from,
    const uint32_t amount)
{
    #ifndef IGNORE_ASSERTS 
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

char * consume_till_terminate(
    DataStream * from,
    uint32_t max_size,
    char terminator)
{
    #ifndef IGNORE_ASSERTS
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
    
    #ifndef IGNORE_ASSERTS
    assert(string_size > 0);
    #endif
    
    char * return_value = malloc(
        string_size * sizeof(char));
    
    for (int i = 0; i <= string_size; i++) {
        #ifndef IGNORE_ASSERTS
        assert(from->size_left > 0);
        #endif
        return_value[i] = ((char *)from->data)[0];
        from->data++;
        from->size_left--;
    }
    
    return return_value;
}

/*
Our hashmaps are full of pointers to heap memory,
this frees everything in 1 go
*/
void free_hashed_huff(HashedHuffman * dict)
{
    if (dict == NULL) { return; }
    
    for (int i = 0; i < HUFFMAN_HASHMAP_SIZE; i++) {
        while (true)
        {
            if (dict->entries[i] == NULL) { break; }
            if (dict->entries[i]->next_neighbor == NULL) {
                break;
            }
            if (dict->entries[i]->next_neighbor->next_neighbor
                == NULL) {
                break;
            }
            
            HashedHuffmanEntry * last_neighbor =
                dict->entries[i]->next_neighbor;
            
            while (
                last_neighbor->next_neighbor != NULL
                && last_neighbor->next_neighbor->next_neighbor
                    != NULL)
            {
                last_neighbor =
                    last_neighbor->next_neighbor;
            }
           
            #ifndef IGNORE_ASSERTS 
            assert(last_neighbor->next_neighbor != NULL);
            assert(
                last_neighbor->next_neighbor->next_neighbor
                    == NULL);
            #endif
            free(last_neighbor->next_neighbor);
            last_neighbor->next_neighbor = NULL;
            #ifndef IGNORE_ASSERTS
            assert(last_neighbor->next_neighbor == NULL);
            #endif
        }
        
        if (dict->entries[i] != NULL
            && dict->entries[i]->next_neighbor != NULL)
        {
            #ifndef IGNORE_ASSERTS
            assert(dict->entries[i]->next_neighbor != NULL);
            assert(
                dict->entries[i]->next_neighbor->next_neighbor
                    == NULL);
            #endif
            free(dict->entries[i]->next_neighbor);
            dict->entries[i]->next_neighbor = NULL;
            #ifndef IGNORE_ASSERTS
            assert(dict->entries[i]->next_neighbor == NULL);
            #endif
        }
        
        if (dict->entries[i] != NULL) {
            #ifndef IGNORE_ASSERTS
            assert(dict->entries[i]->next_neighbor == NULL);
            #endif
            free(dict->entries[i]);
            dict->entries[i] = NULL;
        }
    }
    
    free(dict);
    dict = NULL;
}

/*
Given a datastream and a hashmap of huffman codes,
read & decompress/decode the next value
*/
uint32_t hashed_huffman_decode(
    HashedHuffman * dict,
    DataStream * datastream)
{
    #ifndef IGNORE_ASSERTS
    assert(dict != NULL);
    assert(datastream != NULL);
    #endif
    
    unsigned int bitcount = dict->min_code_length - 1;
    uint32_t upcoming_bits =
        peek_bits(
            /* from: */ datastream,
            /* size: */ 24);
    uint32_t raw = 0;
    
    while (bitcount < dict->max_code_length)
    {
        bitcount += 1;
        
        /*
        Spec:
        "Huffman codes are packed starting with the most-
            significant bit of the code."
        
        Attention: The hash keys are already reversed,
        so we can just compare the raw values to the keys 
        */
        raw = mask_rightmost_bits(
            upcoming_bits,
            bitcount);
        
        uint32_t hash = compute_hash(
            /* key: */ raw,
            /* code_length: */ bitcount);
        
        if (dict->entries[hash] == NULL)
        {
            continue;
        } else if (
            dict->entries[hash]->key == raw
            && dict->entries[hash]->code_length == bitcount)
        {
            discard_bits(datastream, bitcount);
            return dict->entries[hash]->value;
        } else {
            HashedHuffmanEntry * next_neighbor =
                dict->entries[hash]->next_neighbor;
            while (next_neighbor != NULL) {
                if (next_neighbor->key == raw
                    && next_neighbor->code_length == bitcount)
                {
                    discard_bits(datastream, bitcount);
                    return next_neighbor->value;
                } else {
                    next_neighbor =
                        next_neighbor->next_neighbor;
                }
            }
        }
    }
    
    #ifndef INFLATE_SILENCE 
    printf(
        "failed to find raw :%u for codelen: %u in dict\n",
        raw,
        bitcount);
    #endif
    
    crash_program();
    
    return 0;
}

/*
Convert an array of huffman codes to a hashmap of huffman codes
*/
HashedHuffman * huffman_to_hashmap(
    HuffmanEntry * orig_huff,
    uint32_t orig_huff_size)
{
    unsigned int longest_conflict = 0;
    
    HashedHuffman * hashed_huffman =
        malloc(sizeof(HashedHuffman));
   
    // initialize all pointers to NULL 
    for (unsigned int i = 0; i < HUFFMAN_HASHMAP_SIZE; i++)
    {
        hashed_huffman->entries[i] = NULL;
    }
    
    hashed_huffman->min_code_length = 1000;
    hashed_huffman->max_code_length = 1;
    
    for (unsigned int i = 0; i < orig_huff_size; i++)
    {
        if (orig_huff[i].used == false) {
            continue;
        }
        
        // we'll store the reversed key, so that we don't
        // have to reverse on each lookup
        uint32_t reversed_key = reverse_bit_order(
            /* raw: */ orig_huff[i].key,
            /* size: */ orig_huff[i].code_length);
        
        uint32_t hash = compute_hash(
            /* key: */ reversed_key,
            /* code_length: */ orig_huff[i].code_length);

        #ifndef IGNORE_ASSERTS
        assert(hash <= HUFFMAN_HASHMAP_SIZE);
        assert(hash >= 0);
        #endif
        
        if (orig_huff[i].code_length
            < hashed_huffman->min_code_length)
        {
            hashed_huffman->min_code_length =
                orig_huff[i].code_length;
        }
        if (orig_huff[i].code_length
            > hashed_huffman->max_code_length)
        {
            hashed_huffman->max_code_length =
                orig_huff[i].code_length;
        }
        
        if (
            hashed_huffman->entries[hash] == NULL)
        {
            hashed_huffman->entries[hash] =
                malloc(sizeof(HashedHuffmanEntry));
            
            // first time using this hash
            hashed_huffman->entries[hash]->key = reversed_key;
            hashed_huffman->entries[hash]->code_length =
                orig_huff[i].code_length;
            hashed_huffman->entries[hash]->value =
                orig_huff[i].value;
            hashed_huffman->entries[hash]->next_neighbor = NULL;
        } else {
            // hash conflicting, append new value to linked list
            HashedHuffmanEntry * last_full_link = 
                hashed_huffman->entries[hash];
            #ifndef IGNORE_ASSERTS
            assert(last_full_link != NULL);
            #endif
            
            unsigned current_conflict = 1;
            
            while (last_full_link->next_neighbor != NULL)
            {
                last_full_link = last_full_link->next_neighbor;
                current_conflict++;
            }
           
            #ifndef IGNORE_ASSERTS 
            assert(last_full_link->next_neighbor == NULL);
            #endif
            
            last_full_link->next_neighbor =
                malloc(sizeof(HashedHuffmanEntry));
            last_full_link->next_neighbor->next_neighbor =
                NULL;
            last_full_link->next_neighbor->key =
                reversed_key;
            last_full_link->next_neighbor->code_length =
                orig_huff[i].code_length;
            last_full_link->next_neighbor->value =
                orig_huff[i].value;
            
            if (current_conflict > longest_conflict) {
                longest_conflict = current_conflict;
            }
        }
    }
   
    #ifndef IGNORE_ASSERTS 
    assert(
        hashed_huffman->min_code_length
            <= hashed_huffman->max_code_length);
    #endif
    
    return hashed_huffman;
}

/*
Given an array of code lengths, unpack it to
an array of huffman codes
*/
HuffmanEntry * unpack_huffman(
    uint32_t * array,
    const uint32_t array_size)
{
    HuffmanEntry * unpacked_dict = malloc(
        sizeof(HuffmanEntry) * array_size);
    
    // initialize dict
    for (int i = 0; i < array_size; i++) {
        unpacked_dict[i].value = i;
        unpacked_dict[i].code_length = array[i];
        unpacked_dict[i].key = 1234543;
        unpacked_dict[i].used = false;
    }
    
    // 1) Count the number of codes for each code length.  Let
    // bl_count[N] be the number of codes of length N, N >= 1.
    uint32_t * bl_count = malloc(
        array_size * sizeof(uint32_t));
    unsigned int unique_code_lengths = 0;
    unsigned int min_code_length = 123454321;
    unsigned int max_code_length = 0;
    for (int i = 0; i < array_size; i++) {
        bl_count[i] = 0;
    }
    
    for (int i = 0; i < array_size; i++) {
        if (bl_count[array[i]] == 0) {
            unique_code_lengths += 1;
        }
        if (array[i] > max_code_length) {
            max_code_length = array[i];
        }
        if (array[i] < min_code_length && array[i] > 0) {
            min_code_length = array[i];
        }
        bl_count[array[i]] += 1;
    }
    
    // Spec: 
    // 2) "Find the numerical value of the smallest code for each
    //    code length:"
    uint32_t * smallest_code =
        malloc(array_size * sizeof(uint32_t));
    
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
   
    #ifndef IGNORE_ASSERTS 
    assert(max_code_length < array_size);
    #endif
    
    for (
        int bits = 1;
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
            bool32_t actually_used = false;
            for (int i = 0; i < array_size; i++) {
                if (unpacked_dict[i].code_length == bits) {
                    actually_used = true;
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
                
                crash_program();
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
        
        if (len >= min_code_length) {
            unpacked_dict[n].key = smallest_code[len];
            unpacked_dict[n].used = true;
            
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
void inflate(
    uint8_t * recipient,
    uint32_t recipient_size,
    DataStream * data_stream,
    unsigned int compressed_size_bytes) 
{
    #ifndef IGNORE_ASSERTS
    assert(recipient != NULL);
    assert(recipient_size >= compressed_size_bytes);
    assert(data_stream != NULL);
    assert(data_stream->data != NULL);
    assert(data_stream->size_left > 0);
    assert(compressed_size_bytes > 0);
    #endif
    
    #ifndef INFLATE_SILENCE
    printf(
        "\t\tstart INFLATE expecting %u bytes of compr. data\n",
        compressed_size_bytes);
    #endif
    void * started_at = data_stream->data;
    uint8_t * recipient_at = recipient;
   
    #ifndef IGNORE_ASSERTS 
    assert(data_stream->size_left >= compressed_size_bytes);
    assert(data_stream->bits_left == 0);
    #endif
    
    int read_more_deflate_blocks = true;
    
    while (read_more_deflate_blocks) {
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
        uint8_t BFINAL = consume_bits(
            /* buffer: */ data_stream,
            /* size  : */ 1);
        #ifndef IGNORE_ASSERTS
        assert(BFINAL < 2);
        #endif
        
        #ifndef INFLATE_SILENCE
        printf(
            "\t\t\tBFINAL (flag for final block): %u\n",
            BFINAL);
        #endif
        if (BFINAL) { read_more_deflate_blocks = false; }
        
        uint8_t BTYPE =  consume_bits(
            /* buffer: */ data_stream,
            /* size  : */ 2);
        
        if (BTYPE == 0) {
            #ifndef INFLATE_SILENCE
            printf("\t\t\tBTYPE 0 - No compression\n");
            #endif
            
            // spec says to ditch remaining bits
            if (data_stream->bits_left > 0) {
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tditching a byte with %u%s\n",
                    data_stream->bits_left,
                    " bits left...");
                #endif
                
                discard_bits(
                    /* from: */ data_stream,
                    /* amount: */ data_stream->bits_left);
                #ifndef IGNORE_ASSERTS
                assert(data_stream->bits_left == 0);
                #endif
            }
            
            uint16_t LEN =
                (int16_t)consume_bits(data_stream, 16);
            #ifndef INFLATE_SILENCE
            printf(
                "\t\t\tuncompr. block has LEN: %u bytes\n",
                LEN);
            #endif
            
            uint16_t NLEN =
                (uint16_t)consume_bits(data_stream, 16);

            #ifndef IGNORE_ASSERTS
            // spec says must be 1's complement of LEN
            assert((uint16_t)LEN == (uint16_t)~NLEN);
            #endif
            
            for (int _ = 0; _ < LEN; _++) {
                *recipient_at = *(uint8_t *)data_stream->data;
                recipient_at++;
                #ifndef IGNORE_ASSERTS
                assert(
                    (recipient_at - recipient)
                        <= recipient_size);
                #endif
                data_stream->data++;
                data_stream->size_left--;
            }
        } else if (BTYPE > 2) {
            #ifndef INFLATE_SILENCE
            printf(
                "\t\t\tERROR - unexpected deflate BTYPE %u\n",
                BTYPE);
            #endif
            crash_program();
        } else {
            #ifndef IGNORE_ASSERTS
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
                
                #ifndef IGNORE_ASSERTS
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
                
                literal_length_huffman =
                    unpack_huffman(
                        /* array:     : */
                            fixed_hclen_table,
                        /* array_size : */
                            288);
               
                #ifndef IGNORE_ASSERTS 
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

                hashed_litlen_huffman =
                    huffman_to_hashmap(
                        /* original: */ literal_length_huffman,
                        /* orig_size: */ HLIT);
                
                #ifndef INFLATE_SILENCE 
                printf("\t\t\tcreated fixed huffman dict.\n");
                #endif
            } else {
                #ifndef IGNORE_ASSERTS
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
                    /* from: */ data_stream,
                    /* size: */ 5)
                        + 257;
                
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tHLIT : %u (expect 257-286)\n",
                    HLIT);
                #endif

                #ifndef IGNORE_ASSERTS
                assert(HLIT >= 257 && HLIT <= 286);
                #endif
                
                // 5 Bits: HDIST (huffman distance?)
                // # of Distance codes - 1
                // (1 - 32)
                HDIST = consume_bits(
                    /* from: */ data_stream,
                    /* size: */ 5)
                        + 1;
                
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tHDIST: %u (expect 1-32)\n",
                    HDIST);
                #endif

                #ifndef IGNORE_ASSERTS
                assert(HDIST >= 1 && HDIST <= 32);
                #endif
                
                // 4 Bits: HCLEN (huffman code length)
                // # of Code Length codes - 4
                // (4 - 19)
                uint32_t HCLEN = consume_bits(
                    /* from: */ data_stream,
                    /* size: */ 4)
                        + 4;
                
                #ifndef INFLATE_SILENCE
                printf(
                    "\t\t\tHCLEN: %u (4-19 vals of 0-6)\n",
                     HCLEN);
                #endif
               
                #ifndef IGNORE_ASSERTS 
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
                    #ifndef IGNORE_ASSERTS
                    assert(swizzle[i] <
                        NUM_UNIQUE_CODELENGTHS);
                    #endif
                    
                    HCLEN_table[swizzle[i]] =
                            consume_bits(
                                /* from: */ data_stream,
                                /* size: */ 3);
                    
                    #ifndef IGNORE_ASSERTS
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
                
                // TODO: should I do NUM_UNIQUE_CODELGNTHS
                // or only HCLEN? 
                HuffmanEntry * codelengths_huffman =
                    unpack_huffman(
                        /* array:     : */
                            HCLEN_table,
                        /* array_size : */
                            NUM_UNIQUE_CODELENGTHS);
                HashedHuffman * hashed_clen_huffman =
                    huffman_to_hashmap(
                        /* original dict: */
                            codelengths_huffman,
                        /* original size: */
                            NUM_UNIQUE_CODELENGTHS);
                
                for (
                    int i = 0;
                    i < NUM_UNIQUE_CODELENGTHS;
                    i++)
                {
                    if (codelengths_huffman[i].used == true) {
                        #ifndef IGNORE_ASSERTS
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
                
                uint32_t * litlendist_table = malloc(
                    sizeof(uint32_t) * two_dicts_size);
                
                while (len_i < two_dicts_size) {
                    uint32_t encoded_len = hashed_huffman_decode(
                        /* dict: */
                            hashed_clen_huffman,
                        /* raw data: */
                            data_stream);
                    
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
                                /* from: */ data_stream,
                                /* size: */ 2);
                        uint32_t repeats =
                            extra_bits_repeat + 3;
                       
                        #ifndef IGNORE_ASSERTS 
                        assert(repeats >= 3);
                        assert(repeats <= 6);
                        assert(len_i > 0);
                        #endif
                        
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
                        17: Repeat a code length of 0 for 3 - 10
                            times.
                            3 extra bits for length
                        */
                        uint32_t extra_bits_repeat =
                            consume_bits(
                                /* from: */ data_stream,
                                /* size: */ 3);
                        uint32_t repeats =
                            extra_bits_repeat + 3;
                       
                        #ifndef IGNORE_ASSERTS 
                        assert(repeats >= 3);
                        assert(repeats < 11);
                        #endif
                        
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
                        18: Repeat a code length of 0 for
                            11 - 138 times
                            7 extra bits for length
                        */
                        uint32_t extra_bits_repeat =
                            consume_bits(
                                /* from: */ data_stream,
                                /* size: */ 7);
                        uint32_t repeats =
                            extra_bits_repeat + 11;
                        
                        #ifndef IGNORE_ASSERTS
                        assert(repeats >= 11);
                        assert(repeats < 139);
                        #endif
                        
                        for (
                            int i = 0;
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
                        crash_program();
                    }
                }
                
                #ifndef INFLATE_SILENCE
                printf("\t\t\tfinished reading two dicts\n");
                #endif
                
                #ifndef IGNORE_ASSERTS
                assert(len_i == two_dicts_size);
                #endif
                
                literal_length_huffman =
                    unpack_huffman(
                        /* array:     : */
                            litlendist_table,
                        /* array_size : */
                            HLIT);
                hashed_litlen_huffman =
                    huffman_to_hashmap(
                        /* original: */ literal_length_huffman,
                        /* orig_size: */ HLIT);
                
                #ifndef INFLATE_SILENCE 
                printf(
                    "\t\t\tunpacked lit/len dict\n");
                #endif
                
                for (int i = 0; i < HLIT; i++) {
                    if (literal_length_huffman[i].used == true) {
                        #ifndef IGNORE_ASSERTS
                        assert(
                            literal_length_huffman[i].value
                                == i);
                        assert(
                            literal_length_huffman[i].key
                                < 99999);
                        #endif
                    }
                }
                
                distance_huffman =
                    unpack_huffman(
                        /* array:     : */
                            litlendist_table + HLIT,
                        /* array_size : */
                            HDIST);
                hashed_dist_huffman =
                    huffman_to_hashmap(
                        /* original dict: */ distance_huffman,
                        /* original size: */ HDIST);
                
                #ifndef INFLATE_SILENCE
                printf("\t\t\tunpacked distance dictionary\n");
                #endif
                
                for (int i = 0; i < HDIST; i++) {
                    if (distance_huffman[i].used == true) {
                        #ifndef IGNORE_ASSERTS
                        assert(distance_huffman[i].value == i);
                        assert(distance_huffman[i].key < 99999);
                        #endif
                    }
                }
                
                free(codelengths_huffman);
                free_hashed_huff(hashed_clen_huffman);
                free(litlendist_table);
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
                #ifndef IGNORE_ASSERTS
                assert(distance_huffman != NULL);
                assert(HDIST > 0);
                assert(HLIT > 0);
                assert(HLIT < 300);
                #endif
            } else if (BTYPE == 1) {
                #ifndef IGNORE_ASSERTS
                assert(distance_huffman == NULL);
                assert(HDIST == 0);
                assert(HLIT == 288);
                #endif
            }
            
            while (1) {
                // we should normally break from this loop
                // because we hit the magical value 256,
                // not because of running out of bytes
                
                if (data_stream->data - started_at
                    >= compressed_size_bytes)
                {
                    #ifndef INFLATE_SILENCE
                    printf(
                        "\t\tWarning: breaking from DEFLATE preemptively because %li bytes were read\n",
                        data_stream->data - started_at);
                    printf(
                        "\t\tcompressed_size_bytes was: %u\n",
                        compressed_size_bytes);
                    #endif
                    read_more_deflate_blocks = false;
                    break;
                }
                
                uint32_t litlenvalue = hashed_huffman_decode(
                    /* dict: */
                        hashed_litlen_huffman,
                    /* raw data: */
                        data_stream);
                
                if (litlenvalue < 256) {
                    // literal value, not a length
                    *recipient_at =
                        (uint8_t)(litlenvalue & 255);
                    recipient_at++;
                    
                    #ifndef IGNORE_ASSERTS
                    assert(
                        (recipient_at - recipient)
                            <= recipient_size);
                    #endif
                    
                } else if (litlenvalue > 256) {
                    // length, (therefore also need distance)
                    #ifndef IGNORE_ASSERTS
                    assert(litlenvalue < 286);
                    #endif
                    uint32_t i = litlenvalue - 257;
                    
                    #ifndef IGNORE_ASSERTS
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
                                /* from: */ data_stream,
                                /* size: */ extra_bits)
                        : 0;
                    uint32_t total_length =
                        base_length + extra_length;

                    #ifndef IGNORE_ASSERTS
                    assert(
                        total_length >=
                        length_extra_bits_table[i]
                            .base_decoded);
                    #endif
                    
                    uint32_t distvalue =
                        distance_huffman == NULL ?
                            reverse_bit_order(
                                consume_bits(
                                    /* from: */ data_stream,
                                    /* size: */ 5),
                                5)
                        : hashed_huffman_decode(
                            /* dict: */
                                 hashed_dist_huffman,
                            /* raw data: */
                                 data_stream);
                   
                    #ifndef IGNORE_ASSERTS 
                    assert(distvalue <= 29);
                    assert(
                        dist_extra_bits_table[distvalue].value
                            == distvalue);
                    #endif
                    
                    uint32_t dist_extra_bits =
                        dist_extra_bits_table[distvalue]
                            .num_extra_bits;
                    uint32_t base_dist =
                        dist_extra_bits_table[distvalue]
                            .base_decoded;
                    
                    uint32_t dist_extra_bits_decoded =
                        dist_extra_bits > 0 ?
                            consume_bits(
                                /* from: */ data_stream,
                                /* size: */ dist_extra_bits)
                            : 0;
                    
                    uint32_t total_dist =
                        base_dist + dist_extra_bits_decoded;
                    
                    // go back dist bytes, then copy length bytes
                    #ifndef IGNORE_ASSERTS
                    assert(
                        recipient_at - total_dist >= recipient);
                    #endif
                    uint8_t * back_dist_bytes =
                        recipient_at - total_dist;
                    for (int _ = 0; _ < total_length; _++) {
                        *recipient_at = *back_dist_bytes;
                        recipient_at++;
                        #ifndef IGNORE_ASSERTS
                        assert(
                            (recipient_at - recipient)
                                < recipient_size);
                        #endif
                        back_dist_bytes++;
                    }
                } else {
                    #ifndef IGNORE_ASSERTS
                    assert(litlenvalue == 256);
                    #endif
                    
                    #ifndef INFLATE_SILENCE
                    printf("\t\tend of ltln found!\n");
                    #endif
                    
                    break;
                }
            }
            
            #ifndef INFLATE_SILENCE
            printf("\t\tend of DEFLATE block...\n");
            #endif
            
            free(literal_length_huffman);
            free_hashed_huff(hashed_litlen_huffman);
	    free(distance_huffman);
	    free_hashed_huff(hashed_dist_huffman);
        }
    }

    if (data_stream->bits_left != 0) {
        #ifndef INFLATE_SILENCE
        printf(
            "\t\tpartial byte left after DEFLATE\n");
        printf(
            "\t\tdiscarding: %u bits\n",
            data_stream->bits_left);
        #endif
        
        discard_bits(
            /* from: */ data_stream,
            /* amount: */ data_stream->bits_left);
    }
    
    int bytes_read = data_stream->data - started_at; 
    #ifndef IGNORE_ASSERTS
    assert(bytes_read >= 0);
    #endif
    if (bytes_read != compressed_size_bytes) {
        #ifndef INFLATE_SILENCE
        printf(
            "Warning: expected to read %u bytes but got %u\n",
            compressed_size_bytes,
            bytes_read);
        #endif
        
        #ifndef IGNORE_ASSERTS
        assert(compressed_size_bytes > bytes_read);
        #endif
        
        int skip = compressed_size_bytes - bytes_read;
        #ifndef INFLATE_SILENCE
        printf("skipping ahead %u bytes...\n", skip);
        #endif

        #ifndef IGNORE_ASSERTS
        assert(data_stream->size_left >= skip);
        #endif
        
        data_stream->data += skip;
        data_stream->size_left -= skip;
    }
   
    #ifndef INFLATE_SILENCE 
    printf("\t\tend of DEFLATE\n");
    #endif
}

#undef true
#undef false

