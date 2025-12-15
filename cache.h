#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

extern uint64_t g_lru_counter;

struct CacheLine {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    uint64_t lru = 0;
};

struct AddressInfo {
    uint32_t tag;
    uint32_t index;
};

struct CacheSet {
    std::vector<CacheLine> lines; 
};
class Cache {
protected:
    int size;
    int block_size;
    int assoc;
    int num_sets;
    int cycles;
    Cache* next_level_cache = nullptr;

    std::vector<CacheSet> sets;

public:
    Cache(int size_log2, int block_size_log2, int assoc_log2, int cycles) 
        : cycles(cycles) {
        if (size_log2 < 0 || block_size_log2 < 0 || assoc_log2 < 0) {
            throw std::invalid_argument("Cache parameters must be non-negative powers of 2 (log2 values).");
        }
        //This actions are like to do 2^(val)
        this->size = 1 << size_log2; 
        this->block_size = 1 << block_size_log2; 
        this->assoc = 1 << assoc_log2;
        int set_size = this->block_size * this->assoc;
        if (set_size == 0 || this->size % set_size != 0) {
            throw std::invalid_argument("Cache parameters lead to invalid set configuration.");
        }
        this->num_sets = this->size / set_size;
        this->sets.reserve(this->num_sets);
    }

    AddressInfo decomposeAddress(uint32_t address) {
            
            // 1. Calculate Block Offset Bits (b_bits)
            int b_bits = 0;
            if (block_size > 0) {
                b_bits = std::log2(block_size); 
            }

            // 2. Calculate Set Index Bits (s_bits)
            int num_sets = size / (block_size * assoc);
            int s_bits = 0;
            if (num_sets > 0) {
                s_bits = std::log2(num_sets);
            }
            // 3. Calculate Tag Size (t_bits)
            const int TOTAL_BITS = 32;
            int t_bits = TOTAL_BITS - (b_bits + s_bits);

            // 4. Extract Index Value 
            uint32_t index = (address >> b_bits) & ((1 << s_bits) - 1);
            
            // 5. Extract Tag Value 
            uint32_t tag = address >> (b_bits + s_bits);

            return {tag, index};
        }
    uint32_t reconstructAddress(uint32_t tag, uint32_t index) {
        
        // 1. Calculate Block Offset Bits (b_bits) - Same logic as decomposeAddress
        int b_bits = 0;
        if (block_size > 0) {
            b_bits = std::log2(block_size); 
        }

        // 2. Calculate Set Index Bits (s_bits) - Same logic as decomposeAddress
        // NOTE: It is important that this calculation is consistent!
        int num_sets = size / (block_size * assoc);
        int s_bits = 0;
        if (num_sets > 0) {
            s_bits = std::log2(num_sets);
        }

        // 3. Reconstruct the address using bit shifting
        
        // Total bits for Index and Offset sections
        const int IS_BITS = b_bits + s_bits; 

        // Shift the Tag into its high position: Tag << (s_bits + b_bits)
        // The tag is shifted left by the total number of index and offset bits.
        uint32_t tag_part = tag << IS_BITS;

        // Shift the Index into its position: Index << b_bits
        // The index is shifted left by the number of offset bits.
        uint32_t index_part = index << b_bits;

        // The reconstructed address is the sum (or bitwise OR) of the Tag part and the Index part.
        // We assume offset is 0 as we want the address of the start of the block.
        uint32_t address = tag_part | index_part; 

        return address;
    }
    
    virtual double lookup(uint32_t address, bool is_write, bool &hit) = 0; 
    virtual void insert(uint32_t address, bool is_write) = 0;
};

