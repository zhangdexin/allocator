#include <iostream>

#include "DynamicAllocator.h"

int main()
{
    DynamicAllocator alloctor;
    void* bp1 = alloctor.memMalloc(33);
    void* bp2 = alloctor.memMalloc(3);
    void* bp3 = alloctor.memMalloc(3);

    alloctor.memFree(bp1);
    alloctor.memFree(bp3);
    alloctor.memFree(bp2);

    return 0;
}