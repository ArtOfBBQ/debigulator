typedef int32_t bool32_t;
#define false 0
#define true 1

#define NUM_UNIQUE_CODELENGTHS 19

typedef struct EntireFile {
    void * data;
    
    uint8_t bits_left;
    uint8_t bit_buffer;
    
    size_t size_left;
} EntireFile;

typedef struct HuffmanEntry {
    uint32_t key;
    uint32_t code_length;
    uint32_t value;
    bool32_t used;
} HuffmanEntry;

uint32_t mask_rightmost_bits(
    const uint32_t input,
    const int bits_to_mask)
{
    int mask = 0;
    int next_increment = 1;
    for (int _ = 0; _ < bits_to_mask; _++) {
        mask += next_increment;
        next_increment *= 2;
    }
    
    return input & mask;
}

uint32_t reverse_bit_order(
    const uint32_t original,
    const unsigned int bit_count)
{
    assert(bit_count > 0);
    assert(bit_count < 33);
    
    if (bit_count == 1) { return original; }
    
    uint32_t return_value = 0;
    
    float middle = (bit_count - 1.0f) / 2.0f;
    assert(middle > 0);
    assert(middle <= 16.0f);
    
    for (uint32_t i = 0; i < bit_count; i++) {
        float dist_to_middle = middle - i;
        assert(dist_to_middle < 16.0f);
        
        uint8_t target_pos = i + (dist_to_middle * 2);
        
        assert(target_pos >= 0);
        assert(target_pos < 33);
        
        return_value =
            return_value |
                (((original >> i) & 1) << target_pos);
    }
    
    return return_value;
}

uint32_t peek_bits(
    EntireFile * from,
    const uint32_t amount)
{
    assert(amount > 0);
    assert(amount < 33);
    assert(from->bits_left < 9);
    
    uint32_t return_value = 0;
    uint32_t bits_to_peek = amount;
    
    int bits_in_return = 0;
    
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
    
    assert(bits_to_peek < 8);
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

void discard_bits(
    EntireFile * from,
    const unsigned int amount)
{
    assert(from->bits_left < 9);
    assert(amount > 0);
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
        assert(from->bits_left == 0);
        from->data += 1;
        from->size_left -= 1;
        discards_left -= 8;
    }
    
    assert(discards_left < 9);
    if (discards_left > 0) {
        assert(from->bits_left == 0);
        from->bit_buffer = *(uint8_t *)from->data;
        from->data += 1;
        from->size_left -= 1;
        from->bits_left = (8 - discards_left);
        from->bit_buffer >>= discards_left;
    }
    
    assert(from->bits_left < 9);
}

uint32_t consume_bits(
    EntireFile * from,
    const uint32_t amount)
{
    assert(amount > 0);
    assert(amount < 33);
    
    uint32_t bits_to_consume = amount;
    
    uint32_t return_val = peek_bits(
        from,
        bits_to_consume);
    discard_bits(
        from,
        bits_to_consume);
    
    return return_val;
}

uint32_t huffman_decode(
    HuffmanEntry * dict,
    const uint32_t dictsize,
    EntireFile * datastream)
{
    int found_at = -1;
    int bitcount = 0;
    uint32_t raw = 0;
    
    while (found_at == -1 && bitcount < 24) {
        bitcount += 1;
        
        /*
        Spec:
        "Huffman codes are packed starting with the most-
            significant bit of the code."
        */
        raw = reverse_bit_order(
            peek_bits(
                /* from: */ datastream,
                /* size: */ bitcount),
            bitcount);
        
        for (int i = 0; i < dictsize; i++) {
            if (dict[i].key == raw
                && dict[i].used == true
                && bitcount == dict[i].code_length)
            {
                found_at = i;
                break;
            }
        }
    }
    
    if (found_at < 0) {
        printf(
            "failed to find raw :%u for codelen: %u in dict\n",
            raw,
            bitcount);
        assert(1 == 2);
    }
    
    discard_bits(datastream, bitcount);
    assert(found_at >= 0);
    assert(found_at < dictsize);
    
    return dict[found_at].value;
};

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
    
    assert(max_code_length < array_size);
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
                printf(
                    "ERROR: smallest_code[%u] was %u%s",
                    bits,
                    smallest_code[bits],
                    " - value can't fit in that few bits!\n");
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

void deflate(
    uint8_t * recipient,
    uint32_t recipient_size,
    EntireFile * entire_file,
    unsigned int compressed_size_bytes) 
{
    assert(recipient != NULL);
    assert(recipient_size >= compressed_size_bytes);
    assert(entire_file != NULL);
    assert(entire_file->data != NULL);
    assert(entire_file->size_left > 0);
    assert(compressed_size_bytes > 0);
    
    printf(
        "\t\trunning DEFLATE algo, expecting %u bytes of compressed data...\n",
        compressed_size_bytes);
    void * started_at = entire_file->data;
    uint8_t * recipient_at = recipient;
    
    assert(entire_file->bits_left == 0);
    assert(entire_file->size_left >= compressed_size_bytes);
    
    int read_more_deflate_blocks = true;
    
    while (read_more_deflate_blocks) {
        printf("\t\treading new DEFLATE block...\n");
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
        if (BFINAL) { read_more_deflate_blocks = false; }
        
        uint8_t BTYPE =  consume_bits(
            /* buffer: */ entire_file,
            /* size  : */ 2);
        
        if (BTYPE == 0) {
            printf("\t\t\tBTYPE 0 - No compression\n");
            
            // spec says to ditch remaining bits
            if (entire_file->bits_left > 0) {
                printf(
                    "\t\t\tditching a byte with %u%s\n",
                    entire_file->bits_left,
                    " bits left...");
                discard_bits(
                    /* from: */ entire_file,
                    /* amount: */ entire_file->bits_left);
                assert(entire_file->bits_left == 0);
            }
            
            // read LEN and NLEN
            uint16_t LEN =
                (int16_t)consume_bits(entire_file, 16);
            printf(
                "\t\t\tuncompr. block has LEN: %u bytes\n",
                LEN);
            uint16_t NLEN =
                (uint16_t)consume_bits(entire_file, 16);
            // spec says must be 1's complement of LEN
            assert((uint16_t)LEN == (uint16_t)~NLEN);
            
            for (int _ = 0; _ < LEN; _++) {
                *recipient_at = *(uint8_t *)entire_file->data;
                recipient_at++;
                assert(
                    (recipient_at - recipient)
                        <= recipient_size);
                entire_file->data++;
                entire_file->size_left--;
            }
        } else if (BTYPE > 2) {
            printf(
                "\t\t\tERROR - unexpected deflate BTYPE %u\n",
                BTYPE);
            assert(1 == 2);
        } else {
            assert(BTYPE >= 1 && BTYPE <= 2);

            // used in both dynamic & fixed huffman encoded files
            HuffmanEntry * literal_length_huffman = NULL;
            
            // only used in dynamic, keep NULL for fixed 
            HuffmanEntry * distance_huffman = NULL;
            
            // will be overwritten in dynamic
            // leave 288 for fixed
            uint32_t HLIT = 288; 
            // only used in dynamic, keep 0 for fixed
            uint32_t HDIST = 0;
            
            if (BTYPE == 1) {
                printf("\t\t\tBTYPE 1 - Fixed Huffman\n");
                assert(distance_huffman == NULL);
                
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
                
                printf("\t\t\tcreated fixed huffman dict.\n");
            } else {
                assert(BTYPE == 2);
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
                HLIT = consume_bits(
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
                HDIST = consume_bits(
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
                assert(
                    HCLEN >= 4
                    && HCLEN <= 19);
                
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
                printf("\t\t\tReading raw code lengths\n");
                
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
                printf("\t\t\tUnpack codelengths table...\n");
                
                // TODO: should I do NUM_UNIQUE_CODELGNTHS
                // or only HCLEN? 
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
                    if (codelengths_huffman[i].used == true) {
                        assert(
                          codelengths_huffman[i].key >= 0);
                        assert(
                          codelengths_huffman[i].value >= 0);
                        assert(
                          codelengths_huffman[i].value < 19);
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
                // TODO: remove this debugging code
                for (int i = 0; i < two_dicts_size; i++) {
                    litlendist_table[i] = 99999;
                }
                
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
                        assert(len_i > 0);
                        
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
                        assert(1 == 2);
                    }
                }
                
                printf("\t\t\tfinished reading two dicts\n");
                assert(len_i == two_dicts_size);
                
                literal_length_huffman =
                    unpack_huffman(
                        /* array:     : */
                            litlendist_table,
                        /* array_size : */
                            HLIT);
                
                printf("\t\t\tunpacked literal/length dictionary\n");
                for (int i = 0; i < HLIT; i++) {
                    if (literal_length_huffman[i].used == true) {
                        assert(
                            literal_length_huffman[i].value == i);
                        assert(
                            literal_length_huffman[i].key < 99999);
                        assert(
                            literal_length_huffman[i].code_length
                                < 15);
                    }
                }
                
                distance_huffman =
                    unpack_huffman(
                        /* array:     : */
                            litlendist_table + HLIT,
                        /* array_size : */
                            HDIST);
                printf("\t\t\tunpacked distance dictionary\n");
                
                for (int i = 0; i < HDIST; i++) {
                    if (distance_huffman[i].used == true) {
                        assert(distance_huffman[i].value == i);
                        assert(distance_huffman[i].key < 99999);
                        assert(distance_huffman[i].code_length < 15);
                    }
                }
                
                free(codelengths_huffman);
                free(litlendist_table);
            }
            
            // the remaining part of the algorithm is mostly
            // the same whether we're using dynamic huffman tables
            // or fixed huffman tables - we just use a different
            // literal-length-dictionary.
            // The only other difference is in the case of fixed
            // huffman, when we need a distance value, it's
            // consumed directly from the stream, whereas for
            // dynamic huffman  it has to be decoded using the
            // distance dictionary we prepared
            if (BTYPE == 2) {
                assert(distance_huffman != NULL);
                assert(HDIST > 0);
                assert(HLIT > 0);
                assert(HLIT < 300);
            } else if (BTYPE == 1) {
                assert(distance_huffman == NULL);
                assert(HDIST == 0);
                assert(HLIT == 288);
            }
            
            while (1) {
                // this consumes from entire_file
                // so we'll eventually hit 256 and break
                uint32_t litlenvalue = huffman_decode(
                    /* dict: */
                        literal_length_huffman,
                    /* dictsize: */
                        HLIT,
                    /* raw data: */
                        entire_file);
                
                if (litlenvalue < 256) {
                    // literal value, not a length
                    *recipient_at =
                        (uint8_t)(litlenvalue & 255);
                    recipient_at++;
                    assert(
                        (recipient_at - recipient)
                            <= recipient_size);
                    
                } else if (litlenvalue > 256) {
                    // length, (therefore also need distance)
                    assert(litlenvalue < 286);
                    uint32_t i = litlenvalue - 257;
                    assert(
                        length_extra_bits_table[i].value
                            == litlenvalue);
                    uint32_t extra_bits =
                        length_extra_bits_table[i]
                            .num_extra_bits;
                    uint32_t base_length =
                        length_extra_bits_table[i]
                            .base_decoded;
                    uint32_t extra_length =
                        extra_bits > 0 ?
                            consume_bits(
                                /* from: */ entire_file,
                                /* size: */ extra_bits)
                        : 0;
                    uint32_t total_length =
                        base_length + extra_length;
                    assert(
                        total_length >=
                        length_extra_bits_table[i]
                            .base_decoded);
                    
                    uint32_t distvalue =
                        distance_huffman == NULL ?
                            reverse_bit_order(
                                consume_bits(
                                    /* from: */ entire_file,
                                    /* size: */ 5),
                                5)
                        : huffman_decode(
                            /* dict: */
                                 distance_huffman,
                            /* dictsize: */
                                 HDIST,
                            /* raw data: */
                                 entire_file);
                    
                    assert(distvalue <= 29);
                    assert(
                        dist_extra_bits_table[distvalue].value
                            == distvalue);
                    uint32_t dist_extra_bits =
                        dist_extra_bits_table[distvalue]
                            .num_extra_bits;
                    uint32_t base_dist =
                        dist_extra_bits_table[distvalue]
                            .base_decoded;
                    
                    uint32_t dist_extra_bits_decoded =
                        dist_extra_bits > 0 ?
                            consume_bits(
                                /* from: */ entire_file,
                                /* size: */ dist_extra_bits)
                            : 0;
                    
                    uint32_t total_dist =
                        base_dist + dist_extra_bits_decoded;
                    
                    // go back dist bytes, then copy length bytes
                    assert(
                        recipient_at - total_dist >= recipient);
                    uint8_t * back_dist_bytes =
                        recipient_at - total_dist;
                    for (int _ = 0; _ < total_length; _++) {
                        *recipient_at = *back_dist_bytes;
                        recipient_at++;
                        assert(
                            (recipient_at - recipient)
                                < recipient_size);
                        back_dist_bytes++;
                    }
                } else {
                    assert(litlenvalue == 256);
                    printf("\t\tend of ltln found!\n");
                    // TODO: figure out what to do
                    // with any remaining bits
                    break;
                }
            }

            printf("\t\tend of DEFLATE block...\n");
            
            free(literal_length_huffman);
            if (distance_huffman != NULL) {
                free(distance_huffman);
            }
        }
    }
    
    printf(
        "\t\tend of DEFLATE, read: %lu bytes\n",
        entire_file->data - started_at);
    printf(
        "\t\tshould have read: %u bytes\n",
        compressed_size_bytes);
    printf(
        "\t\t(decompressed size: %lu bytes)\n",
        recipient_at - recipient);
    printf(
        "\t\tentire_file->bits_left remaining: %u\n",
        entire_file->bits_left);
    assert(entire_file->bits_left == 0);
}

