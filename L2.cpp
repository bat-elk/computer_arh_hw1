#include <cache.h>

class L1Cache;
extern uint64_t g_lru_counter;
extern uint64_t total_mem_writes;
class L2Cache : public Cache {
private:
    int mem_cycles; //DRAM accecss time
    L1Cache* L1_cache_ptr = nullptr;

public:
    L2Cache(int size_log2, int block_size_log2, int assoc_log2, int cycles, int mem_cyc, L1Cache* L1_ptr)
        : Cache(size_log2, block_size_log2, assoc_log2, cycles), 
          mem_cycles(mem_cyc),
          L1_cache_ptr(L1_ptr) { }

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
    
    void L2Cache::insert(uint32_t address, bool is_write) {
        
        AddressInfo info = decomposeAddress(address);
        uint32_t new_tag = info.tag;
        uint32_t index = info.index;
        CacheSet& current_set = sets[index];

        // --- 1. Find the Victim Line (LRU or Invalid) ---
        int victim_index = -1;
        uint64_t min_lru = UINT64_MAX; 

        for (int i = 0; i < current_set.lines.size(); ++i) {
            CacheLine& line = current_set.lines[i];
            
            // A. Prefer an invalid line (empty spot)
            if (!line.valid) {
                victim_index = i;
                break; 
            }

            // B. Find the Least Recently Used line (min lru timestamp)
            if (line.lru < min_lru) {
                min_lru = line.lru;
                victim_index = i;
            }
        }

        CacheLine& victim_line = current_set.lines[victim_index];

        // --- 2. Handle Eviction and Write Back (if victim is valid) ---
        if (victim_line.valid) {
            
            // A. Handle Inclusive Policy: Invalidate the victim in L1
            // This requires L2 to have a pointer to L1.
            if (L1_cache_ptr != nullptr) {
                // Need to get the full address of the block we are kicking out
                uint32_t victim_full_address = reconstructAddress(victim_line.tag, index);
                
                // Invalidate the block in the L1 Cache.
                L1_cache_ptr->invalidate_block(victim_full_address); 
            }
            
            // B. Handle Write Back if the victim is Dirty (L2 is Write Back)
            if (victim_line.dirty) {
                total_mem_writes++;
            }
        }

        // --- 3. Insert the New Block (Always Write Allocate in L2) ---
        
        victim_line.valid = true;
        victim_line.tag = new_tag;
        
        // L2 is Write Allocate: if insertion is due to a Write Miss, it starts dirty.
        victim_line.dirty = is_write; 
        
        // Update LRU counter (this block is the most recently used)
        victim_line.lru = g_lru_counter; 
    }
};