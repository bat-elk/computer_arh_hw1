#include <cache.cpp>

extern uint64_t g_lru_counter;

class L2Cache : public Cache {
private:
    int mem_cycles; //DRAM accecss time

public:
    L2Cache(int size_log2, int block_size_log2, int assoc_log2, int cycles, int mem_cyc)
        : Cache(size_log2, block_size_log2, assoc_log2, cycles), 
          mem_cycles(mem_cyc) { }

    // --- Virtual Methods Implementation ---

    // First, we will look up in the cahce and see if there is hit. If hit ->updating LRU and returning the access time to L2
    // Second, in case of miss -> access to main MEM to L2 
    double L2Cache::lookup(uint32_t address, bool is_write, bool &hit) override {
        // 1. Decompose address into Tag and Index
        AddressInfo info = decomposeAddress(address);
        uint32_t tag = info.tag;
        uint32_t index = info.index;

        // 2. Get the corresponding Cache Set
        CacheSet& current_set = sets[index];
        
        // --- Search Logic ---
        int hit_line_index = -1; 

        // 3. Search the set for the matching Tag
        for (int i = 0; i < current_set.lines.size(); ++i) {
            CacheLine& line = current_set.lines[i];

            if (line.valid && line.tag == tag) {
                hit_line_index = i; // Cache Hit
                break; 
            }
        }

        // --- Hit Case ---
        if (hit_line_index != -1) {
            hit = true;
            CacheLine& line = current_set.lines[hit_line_index];

            // 4. Update LRU: This block was just accessed.
            line.lru = g_lru_counter; 
            
            // 5. Update Dirty bit if Write (L2 is Write Back)
            if (is_write) {
                line.dirty = true;
            }

            // Access time is L2 cycles only.
            // Short Message: L2 Hit
            return (double)this->cycles; 
        } 
        
        // --- Miss Case ---
        else {
            hit = false;
            
            // Short Message: L2 Miss -> Access Main Memory
            
            // 6. Calculate Miss Access Time: t_L2 + t_mem
            double access_time = (double)this->cycles + (double)this->mem_cycles;

            // 7. Insert the new block into L2 (L2 is always Write Allocate)
            // This handles reading the block from Memory and updating L2's state (LRU/Dirty/Victim)
            insert(address, is_write);

            // 8. Return the total miss time
            return access_time;
        }
    }
    
    // L2 insert handles Write Allocate and Write Back policy (setting dirty bit and finding victim).
    void insert(uint32_t address, bool is_write) override;
};