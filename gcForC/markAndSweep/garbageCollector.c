#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heapList.h"
#include "list.h"
#include "gcLogger.h"
#include "garbageCollector.h"

ProgramPtrList* programPtrList;
HeapBlockList* heapBlockList;
HeapBlockList * freeBlockList;
void* freeBlockHeapPtr;
void* heap;
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



void initHeap(size_t size) {
  heap = malloc(size);
  memset(heap, 0, size);
}

void gcDestroy() {
  freeList(programPtrList);
  freeHeapBlockList(heapBlockList);
  freeHeapBlockList(freeBlockList);
  free(heap);
}

void gcInit(size_t size, int log) {
  logMsg("Initializing Garbage Collector", logging);
  logMsg("Allocate a memory block", logging);
  initHeap(size);
  logging = (log ? 1 : 0);
  heapSize = size;
  spaceUsed = 0;
  freeBlockHeapPtr = heap;
  programPtrList = init();
  heapBlockList = initHeapList();
  freeBlockList = initHeapList();
  addNewHeapBlock(freeBlockList, size, heap);
  addNewHeapBlock(heapBlockList, size, heap);
}



void* gcAlloc(size_t ptrSize, void** addr, void** heapParentAddr) {
  printf("addr = %p\n", addr);
  checkPrgPtrExist(addr);
  if (!heap) {
    throwError("Heap doesn't exist!", 1);
    exit(1);
  } else if (heapSize < ptrSize) {
    throwError("Allocation larger than total heap size!", 1);
    exit(1);
  }

  HeapBlock * bthis, bnext;
  for(bthis=freeBlockList->first;bthis;bthis=bnext){
  	bnext=bthis->next;
  	if(bthis->size > ptrSize){
  		break;
  	}
  }

  if(!bthis){
  	garbageCollect();
  }

  for(bthis=freeBlockList->first;bthis;bthis=bnext){
  	bnext=bthis->next;
  	if(bthis->size > ptrSize){
  		break;
  	}
  }
  if(!bthis){
    throwError("ERROR: No space left to allocate memory", 1);
    exit(1);  	
  }
  freeBlockPtr = bthis->heapPtr;
  addNewHeapBlock(heapBlockList, ptrSize, freeBlockHeapPtr);  
  gcReg(addr, heapBlockList->last, heapParentAddr, 1);
  bthis->prev->next = bthis->next;
  char * nextLocation;
  nextLocation = (char *)bthis->heapPtr + ptrSize;
  addNewHeapBlock(freeBlockList, bthis->size - ptrSize, nextLocation);
}

int isReachable(ProgramPtr* ptr) {
  debug("Reachable Call", logging);
  ProgramPtr* progPtr;
  ProgramPtr* next;
  for (progPtr = programPtrList->first; progPtr; progPtr = next) {
    next = progPtr->next;
    if (ptr->heapParent == progPtr->heapBlock) {
      if (progPtr->heapParent && isReachable(progPtr)) {
        return 1;
      }
      else if (!progPtr->heapParent) {
        return 1;
      }
    }
  }
  return 0;
}


void garbageCollect() {
  size_t a;
  ProgramPtr* progPtr = NULL;
  HeapBlock* hPtr = NULL;
  HeapBlock* heapNext = NULL;
  for (progPtr = programPtrList->first; progPtr != NULL; progPtr = progPtr->next) {
    if (progPtr->heapParent != NULL && progPtr->heapBlock != NULL) {
      if (isReachable(progPtr)) {
        progPtr->heapBlock->mark = 1;
      }
    } else {
      if (progPtr->heapBlock != NULL) {
        progPtr->heapBlock->mark = 1;
      }
    }
  }
  for (hPtr = heapBlockList->first; hPtr != NULL; hPtr = heapNext) {
    heapNext = hPtr->next;
    if (hPtr->mark == 0) {
    	addNewHeapBlock(freeBlockList, hptr->heapPtr, hptr->size);	
    	if (hPtr == heapBlockList->first) {
    	    heapBlockList->first = hPtr->next;
    	    if (heapBlockList->listSize > 1) {
    	      hPtr->next->prev = NULL;
    	    }
    	  } else if (hPtr == heapBlockList->last) {
    	    HeapBlock* t = hPtr->prev;
    	    t->next = NULL;
    	         heapBlockList->last = t; 
    	  } else {
    	    HeapBlock* t = hPtr->prev;
    	    HeapBlock* p = hPtr->next;
    	    t->next = p;
    	    p->prev = t; 
    	  }
    	  heapBlockList->listSize--;
    	  free(hPtr);
    	}
  	}	
  	for (hPtr = heapBlockList->first; hPtr; hPtr = heapNext) {
   	 heapNext = hPtr->next;
   	 hPtr->mark = 0;
  }	
}


HeapBlock* getHeapBlockPtr(void** addr) {
  ProgramPtr* ptr;
  ProgramPtr* next;

  for (ptr = programPtrList->first; ptr; ptr = next) {
    next = ptr->next;

    if (addr == (ptr->varAddr)) {
      debug("HeapBlock Ptr found in HeapBlockList", logging);
      /*if (ptr->heapBlock == 0x1) {
        printf("heapblock is 1\n");
        exit(1);
      }*/
      return ptr->heapBlock;
    }
  }

  throwError("Something wrong happened. getHeapBlock", logging);
  return NULL;//should this just exit instead??
}


HeapBlock* setParent(void **addr) {
  HeapBlock* ptr;
  HeapBlock* next;

  for (ptr = heapBlockList->first; ptr; ptr = next) {
    next = ptr->next;

    if (ptr->heapPtr == *addr) {
      // this is the heapblock we are looking for
      debug("Found parent heap block", logging);
      return ptr;
    }
  }

  return NULL;
}

void gcReg(void** addr, HeapBlock* heapBlock, void** heapParentAddr, int isAlloc) {
  ProgramPtr* ptr;

  debug("gcReg", logging);

  if (programPtrList->size == 0) {
    debug("ProgramPtrList is empty, pushing new programPtr", logging);
    push(programPtrList, addr, heapBlock);
  } else {
    // check to see if addr of ptr already exists
    // within programPtrList
    for (ptr = programPtrList->first; ptr != NULL; ptr = ptr->next) {

      if (addr == ptr->varAddr) {
        debug("Found Ptr in ProgramPtrList", logging);
        // this is the ptr we're looking for
        break;
      }
    }

    // if it doesn't exist, push it onto programPtrList
    debug("ptr doesn't exist", logging);
    if (ptr == NULL) {
      push(programPtrList, addr, heapBlock);
      ptr = programPtrList->last;
    }

    if (heapParentAddr != NULL) {
      ptr->heapParent = setParent(heapParentAddr);
    }

    debug("is alloc?", logging);
    if (isAlloc) {
      ptr->heapBlock = heapBlock;
      if (heapBlock == NULL) {
  throwError("NULL heapBlock from gcAlloc!  This should not happen.", logging);
      }
    } else {
      if (*(ptr->varAddr) == NULL) { // TODO: make sure this line is correct.
        logWarning("var is set to null", logging);
        ptr->heapBlock = NULL;
      } else {
        if (heapBlock == NULL) {
          ptr->heapBlock = getHeapBlockPtr(addr);
        } else {
          ptr->heapBlock = heapBlock;
        }
      }
    }

  }
}

void checkPrgPtrExist(void** addr) {
  logWarning("checkProgPtrExist Call", logging);
  ProgramPtr* ptr;
  //ProgramPtr* next;

  for (ptr = programPtrList->first; ptr != NULL; ptr = ptr->next) {

    if (addr == ptr->varAddr) {
      logWarning("Setting heap block to null", logging);
      ptr->heapBlock = NULL;
    }
  }
}

void printProgramPointers(char* msg) {
   logWarning(msg, 1);
   ProgramPtr* pp2; 
   for (pp2 = programPtrList->first; pp2 != NULL; pp2 = pp2->next) {
      printf("(in test)printing programPtr->varAddr: %p\n", pp2->varAddr);
      printf("(in test)printing *(programPtr->varAddr): %p\n", (char*)*(pp2->varAddr));
    }
   logWarning("end debug lines", 1);
   printf("\n");
}