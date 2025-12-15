#include <cache.h>

extern uint64_t g_lru_counter;
class L1Cache : public Cache {
private:
    bool write_allocate_policy; 
public:
    L1Cache(int size, int block_size, int assoc, int cycles, bool wr_alloc, Cache* L2);
    // ...
};