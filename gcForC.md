## 用来记录C语言Garbage Collector的源代码分析。
算法：  
GC标记清除做法的修改版，没有freeMemList，而是压缩存储，确保没有零散的heap block。这种做法会让堆的利用率更高，但是GC的吞吐量会下降。  


使用方法：  
在使用时，首先使用```gcInit(size_t size, int log)```来初始化堆，程序指针链表，空闲内存起始地址。在```malloc```时，改用```gcAlloc(size_t size, void **var, void **heapParentAddr)```来进行```malloc```，在每次gcAlloc的时候，GC会检查是否剩余堆空间足够，如果堆空间不够，就启动GC算法。最后不使用之后需要用gcDestroy来销毁掉GC使用的内存。


程序分析：
**数据结构：**  
```
ProgramPtrList* programPtrList;
HeapBlockList* heapBlockList;
void* heap;
void* freeBlockHeapPtr;
size_t heapSize;
size_t spaceUsed;
int logging;
```

```ProgramPtrList * programPtrList```: 用来记载了所有的变量的信息。里面有:  
```void ** varAddr```，用来指向变量var  
``` HeapBlock * heapBlock```，用来指向这个变量var所指向内存块  
```HeaoBlock * heapParent```，用来指向引用它的内存块  
```Struct ProgramPtr* next, prev```不用说了
 比如， ```int * a = gcAlloc(sizeof(int), &a, NULL)```,那么就会有```void ** varAddr```来指向a。这个原因是，XXXXXXXXXXXXXXXXXXXXXXXXXX。引用它的东西是NULL，意思是root引用它，GC的时候不要free掉。GC会在heap中申请一块sizeof(int)大小的heap block，封装成HeapBlock，让```HeapBlock* heapBlock```指过去。
 
 ```HeapBlockList* heapBlockList;```：用来描述一块堆。里面含有：  
 size_t size：用来记载切出来的这块堆的大小。  
 int mark：在标记-清除过程中来记载这块内存是否是reachable的。如果不是，就需要被sweep掉。  
 ```void * heapPtr```：记录在heap中的内存位置。  
```struct HeapBLock* next, prev```不用说了。

void* heap;：记录我们的维护的堆的起始位置。  
void* freeBlockHeapPtr;：记录我们维护的堆的可以用的起始位置。  
size_t heapSize, spaceUsed：记录我们维护的堆的大小和已经使用了的堆的大小。  
int logging：是一个flag，用来判断是不是在debug模式，中间的信息是否要输出。

**实现过程：**  
首先gcInit(size_t size, int log)，用来开辟一块我们要维护的堆，初始化了heap = malloc(size), logging, heapSIze, spaceUsed=0, freeBlockHeapPtr=heap然后初始化了programPtrList, heapBlockList.

然后是gcAlloc：这个代替了C语言中的malloc。  
 ```gcAlloc(size_t ptrSize, void** addr, void** heapParentAddr)``` ：首先检查这个我们的 ```void  * * addr``` 是不是已经被programPtrList记录过了。也就是说，查看一下这个变量是不是已经指向了一块内存了。如果是的话，那么那块内存就有可能会变成不能reachable的了。但是那个应该是programPtr的属性，不能是heapBlock的。因为可能有若干个变量指向同一块内存。只要有变量能到达那块内存，就不能free。 ```void checkPrgPtrExist(void** addr)```的作用就是遍历一遍programPtrList,然后如果有这个变量的，就把它的heapBLock=NULL。就是说让之前的记录失效。给他在堆中分配空间。如果可以，那就分配，否则调用GC。GC后如果还是内存不够，就报告分配失败。在堆中分配空间，需要：addNewHeapBlock(heapBlockList, ptrSize, freeBlockHeapPtr);在heaoBLockLIst中，添加一块新的记录块，记录其大小，和起始位置（freeBlockHeapPtr)。然后```gcReg(addr, heapBlockList->last, heapParentAddr, 1);。 void gcReg(void** addr, HeapBlock* heapBlock, void** heapParentAddr, int isAlloc)```的功能是，在programPtrList中添加这个var的信息。细节太多暂时忽略不讲。其中重要的一点是调用setParent()。然后，修改freeBlockPtr指针，freeBlockPtr += thisSize。修改spaceUsed。

核心的garbageCollect()：  
首先看函数```isReachable（ProgramPtr* ptr)```：如果指向它的heapBlock（heapParent）和另一个programPtr的heapBlock相同，那么顺着它的父亲继续找。如果它的heapParent=NULL,说明是被一个变量名指过去的，在root中被引用，是reachable。否则，顺着它的父亲一直找过去，发现它的祖先不是NULL，那么他就不是reachable的。
garbageCollect：遍历progPtrList，如果它的父亲堆块是NULL，标记它的堆块。否则如果它是reachable的，标记堆块。否则不标记。然后进入sweep阶段，遍历heapBlockList，如果发现它的mark是0，就把它的右端到freeBlockHeapPtr之间的内容全部提前freeBlockHeaoPtr-(heapPtr+size)。然后修改heapBlockList后面的成员的heapPtr，提前一个size。然后修改programPtr里面的，让里面的变量名字指到现在正确的内存位置。最后，修改spaceUsed，freeBLockHeapPtr。然后删除掉heapBlockList中的这个结点。最后进入reset阶段，把所有的mark位置都取消标记










一些学到的细节点：  
1. void *用来指向内存地址时候，如果涉及到加减计算，我们需要把它先转成char *再进行加减计算以得到byte数，因为void *的算数运算是未定义的。
2. loging的使用
3. 正确的读一个程序的顺序，不是按照代码顺序读，这样卡住或者不理解他在做什么的经历太多了。同样应该从抽象到底层，才能不至于迷失目的，太早被细节耽误。要从上往下读，先读数据结构，再读流程，最后进入每个函数细节
3. 在reachable的地方卡住了，一直想不通它失效后它的父亲block没有被修改过，一直指着某个地方，怎么递归呢。后来意识到for回圈，找不到父亲和儿子的对应也就是相当于没父亲这样做的，for完还找不到就return0了。
4. ```
            if(&(tmpPPtr->varAddr) && tmpPPtr->varAddr) {
              *(tmpPPtr->varAddr) = tmpPPtr->heapBlock->heapPtr;//FIXME: This line is dangerous. 
            }
```这一类的处理方法，确实花了很多时间才读懂。