#ifndef __DYNAMICALLOCATOR_H__
#define __DYNAMICALLOCATOR_H__

#define MemHandler mh
namespace MemHandler
{

static constexpr unsigned MAX_HEAP = 4096U * 64U;
static constexpr unsigned WSIZE = 4U;
static constexpr unsigned DSIZE = 8U;
static constexpr unsigned CHUNKSIZE = (1 << 12);

constexpr unsigned pack(unsigned size, unsigned alloc) {
    return (size | alloc);
}

constexpr unsigned get(void* p) {
    return *(unsigned*)p;
}

constexpr void put(void* p, unsigned val) {
    *(unsigned*)p = val;
}

constexpr unsigned get_size(void* p) {
    return (get(p) & ~0x7);
}

constexpr unsigned get_alloc(void* p) {
    return (get(p) & 0x1);
}

constexpr unsigned get_prev_alloc(void* p) {
    return (get(p) & 0x2);
}

constexpr char* hdrp(void* bp) {
    return (char*)bp - WSIZE;
}

constexpr char* ftrp(void* bp) {
    return (char*)bp + get_size(hdrp(bp)) - DSIZE;
}

constexpr char* next_blkp(void* bp) {
    return (char*)bp + get_size(hdrp(bp));
}

constexpr char* prev_blkp(void* bp) {
    return (char*)bp - get_size((char*)bp - DSIZE);
}

constexpr void set_alloc_to_next_hdrp(void* bp, bool is_alloc) {
    void* next_hdrp = hdrp(next_blkp(bp));
    if (is_alloc) {
        *(unsigned*)next_hdrp = *(unsigned*)next_hdrp | 0x2;
    }
    else {
        *(unsigned*)next_hdrp = *(unsigned*)next_hdrp & ~0x2;
    }
}

};

class DynamicAllocator
{
public:
    DynamicAllocator();
    ~DynamicAllocator();

    void memFree(void* bp);
    void* memMalloc(unsigned size);

private:
    void init();
    void* memBrk(int incr);

    int memInit();
    void* extendHeap(unsigned words);
    void* coalesce(void* bp);
    void* findFit(unsigned asize);
    void place(void* bp, unsigned asize);

private:
    char* mem_heap_     = nullptr;
    char* mem_brk_      = nullptr;
    char* mem_max_addr_ = nullptr;

    void* heap_listp_   = nullptr;
};

#endif // __DYNAMICALLOCATOR_H__