#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heapList.h"
#include "list.h"
#include "gcLogger.h"
#include "garbageCollector.h"

ProgramPtrList* programPtrList;
ProgramPtrList* destProgramPtrList;
HeapBlockList* heapBlockList;
HeapBlockList* destBlockList;
void* freeBlockHeapPtr;
void* heap;
void* dest;
size_t heapSize;
int logging;

HeapBlock* getHeapBlockPtr(void** addr);
HeapBlock* setParent(void **addr);
void gcReg(void** addr, HeapBlock* heapBlock, void** heapParentAddr, int isAlloc);
int isReachable(ProgramPtr* ptr);
void garbageCollect();
void initHeap(size_t size);
void checkPrgPtrExist(void** addr);
void printProgramPointers(char* msg);
void* gcAlloc(size_t ptrSize, void** addr, void** heapParentAddr);
void gcInit(size_t size, int log);
void gcDestroy();
