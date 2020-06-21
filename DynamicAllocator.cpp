#include "DynamicAllocator.h"

#include <iostream>
#include <cstdlib>
#include <algorithm>

DynamicAllocator::DynamicAllocator()
{
    init();
    memInit();
}

DynamicAllocator::~DynamicAllocator()
{
    free(mem_heap_);
}

void DynamicAllocator::memFree(void* bp)
{
    unsigned size = mh::get_size(mh::hdrp(bp));
    unsigned herder_alloc_num = mh::get_prev_alloc(mh::hdrp(bp)) ? 0x2 : 0;
    mh::put(mh::hdrp(bp), mh::pack(size, herder_alloc_num));
    mh::put(mh::ftrp(bp), mh::pack(size, 0));
    mh::set_alloc_to_next_hdrp(bp, false);

    coalesce(bp);
}

void* DynamicAllocator::memMalloc(unsigned size)
{
    unsigned asize = 0;
    if (size == 0) {
        return nullptr;
    }

    if (size <= mh::WSIZE) {
        asize = 2 * mh::WSIZE;
    }
    else {
        asize = (mh::WSIZE + size) % mh::DSIZE == 0 ? 
            (mh::WSIZE + size) : ((mh::WSIZE + size) / mh::DSIZE + 1) * mh::DSIZE;
    }

    void *bp = nullptr;
    if ((bp = findFit(asize)) != nullptr) {
        place(bp, asize);
        return bp;
    }

    unsigned extendSize = std::max(asize, mh::CHUNKSIZE);
    if ((bp = extendHeap(extendSize / mh::WSIZE)) == nullptr) {
        return nullptr;
    }

    place(bp, asize);
    return bp;
}

void DynamicAllocator::init()
{
    mem_heap_ = (char*)malloc(mh::MAX_HEAP);
    mem_brk_ = mem_heap_;
    mem_max_addr_ = (char*)(mem_heap_ + mh::MAX_HEAP);
}

void* DynamicAllocator::memBrk(int incr)
{
    char *old_brk = mem_brk_;
    if (incr < 0 || mem_brk_ + incr > mem_max_addr_) {
        std::cout << "ERROR: memBrk failed, out of memory" << std::endl;
        return nullptr;
    }

    mem_brk_ += incr;
    return old_brk;
}

int DynamicAllocator::memInit()
{
    if ((heap_listp_ = memBrk(4 * mh::WSIZE)) == nullptr) {
        return -1;
    }

    mh::put(heap_listp_, 0);
    mh::put((char*)heap_listp_ + (1 * mh::WSIZE), mh::pack(mh::DSIZE, 1));
    mh::put((char*)heap_listp_ + (2 * mh::WSIZE), mh::pack(mh::DSIZE, 1));
    mh::put((char*)heap_listp_ + (3 * mh::WSIZE), mh::pack(0, 0x3));

    heap_listp_ = (char*)heap_listp_ + 2 * mh::WSIZE;
    if (extendHeap(mh::CHUNKSIZE / mh::WSIZE) == nullptr) {
        return -1;
    }

    return 0;
}

void* DynamicAllocator::extendHeap(unsigned words)
{
    unsigned bytes = (words % 2) ? (words + 1) * mh::WSIZE : words * mh::WSIZE;
    char* bp = (char*)memBrk(bytes);
    if (bp == nullptr) {
        return nullptr;
    }

    // merge last block or epilogue header and init new free block
    unsigned herder_alloc_num = mh::get_prev_alloc(mh::hdrp(bp)) ? 0x2 : 0;
    mh::put(mh::hdrp(bp), mh::pack(bytes, herder_alloc_num)); // block header
    mh::put(mh::ftrp(bp), mh::pack(bytes, 0)); // block footer
    mh::put(mh::hdrp(mh::next_blkp(bp)), mh::pack(0, 1)); // new epilogue header

    return coalesce(bp);
}

void* DynamicAllocator::coalesce(void* bp)
{
    bool prevAlloc = mh::get_prev_alloc(mh::hdrp(bp));
    bool nextAlloc = mh::get_alloc(mh::hdrp(mh::next_blkp(bp)));
    unsigned size = mh::get_size(mh::hdrp(bp));

    if (prevAlloc && nextAlloc) {
        return bp;
    }
    else if (prevAlloc && !nextAlloc) {
        size += mh::get_size(mh::hdrp(mh::next_blkp(bp)));
        mh::put(mh::hdrp(bp), mh::pack(size, 0x2));
        mh::put(mh::ftrp(bp), mh::pack(size, 0));
    }
    else if (!prevAlloc && nextAlloc) {
        size += mh::get_size(mh::hdrp(mh::prev_blkp(bp)));
        mh::put(mh::ftrp(bp), mh::pack(size, 0));

        void* prev_blk = mh::prev_blkp(bp);
        unsigned alloc_num = mh::get_prev_alloc(mh::hdrp(prev_blk)) ? 0x2 : 0;
        mh::put(mh::hdrp(mh::prev_blkp(bp)), mh::pack(size, alloc_num));
        bp = mh::prev_blkp(bp);
    }
    else {
        size += mh::get_size(mh::hdrp(mh::prev_blkp(bp))) +
                mh::get_size(mh::hdrp(mh::next_blkp(bp)));

        void* prev_blk = mh::prev_blkp(bp);
        unsigned alloc_num = mh::get_prev_alloc(mh::hdrp(prev_blk)) ? 0x2 : 0;
        mh::put(mh::hdrp(mh::prev_blkp(bp)), mh::pack(size, alloc_num));
        mh::put(mh::ftrp(mh::next_blkp(bp)), mh::pack(size, 0));
        bp = mh::prev_blkp(bp);
    }

    return bp;
}

void* DynamicAllocator::findFit(unsigned asize)
{
    for (void* bp = heap_listp_; mh::get_size(mh::hdrp(bp)) > 0; bp = mh::next_blkp(bp)) {
        if (!mh::get_alloc(mh::hdrp(bp)) && mh::get_size(mh::hdrp(bp)) >= asize) {
            return bp;
        }
    }

    return nullptr;
}

void DynamicAllocator::place(void* bp, unsigned asize)
{
    unsigned blkSize = mh::get_size(mh::hdrp(bp));
    if (blkSize - asize < mh::DSIZE) {
        mh::put(mh::hdrp(bp), mh::pack(blkSize, 0x3));
        mh::set_alloc_to_next_hdrp(bp, true);
    }
    else {
        mh::put(mh::hdrp(bp), mh::pack(asize, 0x3));
        bp = mh::next_blkp(bp);
        mh::put(mh::hdrp(bp), mh::pack(blkSize - asize, 0x2));
        mh::put(mh::ftrp(bp), mh::pack(blkSize - asize, 0));
    }
}
