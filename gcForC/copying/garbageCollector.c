#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "gcLogger.h"
#include "garbageCollector.h"

ProgramPtrList* programPtrList;
HeapBlockList * heapBlockList;
void *heap;
void * freeBLockHeapPtr;
size_t heapSize;
size_t spaceUsed;
int logging;
/*如果有两个ProgramPtrs指向相同的HeapBlock，则它遍历列表以检索HeapBlock*/
HeapBlock * getHeapBlockPtr(void **addr){
	ProgramPtr * ptr;
	ProgramPtr * next;
	for(ptr = programPtrList->first; ptr; ptr = next){
		next = ptr->next;
		if(addr == (ptr->varAddr)){
			debug("HeapBLock Ptr found in HeapBlockList", logging);
			return ptr->heapBlock;
		}
	}
	throwError("Something wrong happened, getHeapBlock",logging);
	return NULL;
}

HeapBlock * setParent(void **addr){
	HeapBlock * ptr;
	HeapBlock * next;
	for(ptr = heapBlockList->first; ptr; ptr = next){
		next = ptr->next;
		if(ptr->heapPtr == *addr){
			debug("found parent heap block",logging);
			return ptr;
		}
	}
	return NULL;
}

void gcReg(void ** addr, HeapBlock * heapBlock, void ** heapParentAddr, int isAlloc){
	ProgramPtr * ptr;
	debug("gcReg", logging);

	if(programPtrList->size==0){
		debug("ProgramPtrList is empty, pushing new programPtr", logging);
		push(programPtrList, addr, heapBlock);
	}else{
		for(ptr=programPtrList->first; ptr!=NULL; ptr=ptr->next){
			if(addr == ptr->varAddr){
				debug("found ptr in programPtrList",logging);
				brea;
			}
		}
		debug("ptr doesn't exist", logging);
		if(ptr==NULL){
			push(programPtrList, addr, heapBlock);
			ptr = programPtrList->last;
		}
		if(heapParentAddr !=NULL){
			ptr->heapParent = setParent(heapParentAddr);
		}
		debug("is alloc?",logging);
		if(isAlloc){
			ptr->heapBlock = heapBlock;
			if(heapBlock==NULL){
				throwError("NULL heapBLock from gcALloc! htis should not happen",logging);
			}else{
				if(*(ptr-> varAddr)==NULL){
					logWarning("var is set to NULL",logging);
					ptr->heapBlock=NULL;
				}else{
					if(heapBlock == NULL){
						ptr->heapBlock = getHeapBlockPtr(addr);
					}else{
						ptr->heapBlock = heapBlock;
					}
				}
			}
		}
	}
}

int isReachable(ProgramPtr * ptr){
	debug("reachable call", logging);
	ProgramPtr * progPtr;
	ProgramPtr* next;
	debug("is reachable for loop",logging);
	for((progPtr = programPtrList->first; progPtr; progPtr = next)){
		next = progPtr->next;
		if(ptr->heapParent == progPtr->heapBlock){
			debug("passed in ptr heapParent is  equal to itr progptr heapblock",logging);
			if(progPtr->heapParent && isReachable(progPtr)){
				debug("Yes reachable",logging);
				return 1;
			}else if(!progPtr->heapParent){
				debug("yes reachable heapParent null",logging);
				return 1;
			}
		}
	}
	debug("Not reachable", logging);
	return 0;
}

void garbageCollect(){
	size_t a;
	ProgramPtr * progPtr = NULL;
	HeapBlock * hPtr = NULL;
	HeapBLock * heapNext = NULL;

	debug("Mark phase of GC", logging);

	for(progPtr = programPtrList->first; progPtr != NULL; progPtr = progPtr->next){
		debug("mark begin",logging);
		if(progPtr->heapParent!=NULL && progPtr->heapBlock!=NULL){
			debug("is reachable?",logging);
			if(isReachable(progPtr)){
				debug("mark heapBLock 1",logging);
				programPtr->heapBlock->mark = 1;
			}
			//else keep it 0
		}else{
				debug("can mark block 1?",logging);
				if(progPtr->heapBlock !=NULL){
					debug("mark heapBlock 1",logging);
					progPtr->heapBlock->mark = 1;
				}
			}
			debug("mark end",logging);
		}
		debug("mark complete",logging);

		debug("sweep phase of GC",logging);
		for(hPtr = heapBlockList->first; hPtr != NULL; hPtr = heapNext){
			if(logging){
				printf("Start of loop\n");
				printf("Heap Pointer List Size:%d\n", heapBlockList->size);
				printf("HeapBlock Addr: %p\n", hPtr);
			
				if(hPtr->next){
					printf("HeapBlock->next Addr : %p\n",hPtr->next);
				}else{
					printf("hPtr->Next doesn't exist\n");
				}
			}
			heapNext = hPtr->next;

			if(logging){
				printf("heapNext:%p\n",heapNext);
				printf("hPtr->next:%p\n", hPtr->next);
			}

			if(hPtr->mark==0){
				debug("MArk is 0",logging);
				if(logging){
					printf("heapNext:%p\n",heapNext);
					printf("hPtr->next:%p\n",hPtr->next);
				}
				debug("shift",logging);
				long t1 = (char *)freeBLockHeapPtr-(((char *)hPtr->heapPtr) + hPtr->size);
				size_t shiftSize = (size_t)t1;
				size_t zero = 0;
				if(shiftSiiize > zero){
					if(logging){
						printf("shift heap by size :%zu\n",shiftSize);
						printf("heap pointer next:%p\n",hPtr->next->heapPtr);
					}

					if(((char *)(hPtr->next->heapPtr)) >= (((char*)heap)+heapSize)){
						throwError("what have you done ",1);
						exit(1);
					}

					if(((char*)(hPtr->next->heapPtr)+shiftSize) > (((char*)heap) + heapSize)){
						throwError("shift will go outside allocated heap",1);
						exit(1);
					}else if((((char *)(hPtr->next->heapPtr))+shiftSize)==(((char*)heap)+heapSize)){
						logWarning("shift size equals end of heap. Maybe error?",logging);
					}
					memmove(hPtr->heapPtr,hPtr->next->heapPtr,shiftSize);

					if(logging){
						printf("heapNExt:%p\n",heapNext);
						printf("hPtr->next:%p\n",hPtr->next);
					}
					debug("update heapblockList",logging);
					HeapBlock* tmp = hPtr->next;
					debug("About to enter while",logging);
					while(tmp){
						if(tmp->heapPtr){
							char * tmp2 = tmp->heapPtr;
							tmp2 -=hPtr->size;
							tmp->heapPtr = tmp2;
						}
						tmp = tmp->next;
					}
					debug("after heapBLockLIst",logging);
					if(logging){
						printf("heapNext:%p\n",heapNext);
						printf("hPtr->next:%p\n", hPtr->next);
					}

					ProgramPtr* tmpPPtr;
					debug("update program tprs",logging);

					for(tmpPPtr = programPtrList->first;tempPPtr !=NULL; tempPPtr = tempPPtr->next){
						if((tempPPtr->heapBlock)!=NULL && (tempPPtr->heapblock->heapPtr)!=NULL){
							if(&(tempPPtr->varAddr)&&(tempPPtr->varAddr)){
								*(tempPPtr->varAddr) = tempPPtr->heapBlock->heapPtr;
							}else{
								throwError("NULL varAddr!",1);
							}
						}
					}

					debug("after update program ptrs", logging);
					if(logging){
						printf("heapNext:%p\n", heapNext);
						printf("hPtr->next:%p\n",hPtr->next);
					}
				}
					debug("update space used and freeblockheapptr",logging);
					spaceUsed -= hPtr->size;
					char * tmp1 = freeBlockHeapPtr;
					tmp1 -= hPtr->size;
					freeBlockHeapPtr = tmp1;

					debug("remove heapPtr",logging);
					if(logging){
						printf("heapNext:%p\n", heapNext);
						printf("hPtr->next:%p\n",hPtr->next);
					}

					if(hPtr == heapBlockList->first){
						heapBlockList->first = hPtr->next;
						if(heapBlockList->listSize>1){
							hPtr->next->prev=NULL;
						}
					}else if(hPtr==heapBlockList->last){
						debug("last",logging);
						HeapBlock * t = hPtr->prev;
						t->next = NULL;
						heapBLockLIst->last = t;
					}else{
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
void initHeap(size_t size) {
  logMsg("Initializing Heap", logging);
  heap = malloc(size);
  memset(heap, 0, size);
  if (logging) {
    printf("Heap addr: %p\n", heap);
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