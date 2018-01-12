# Notes for [Java Garbage Collection][1]

## 1.初步认识 JVM

### 1.1 分代GC
    eden space(伊甸园空间)
    Survivor Space(幸存空间) S0和S1
    Old Generation(年老代)
    (SE 8中去除了"永久代")

### 1.2 四种引用
    Strong Reference    --不释放
    Soft Reference      --可以释放,但是尽可能作为最后一个释放选项
    Weak Reference      --即将释放
    Phantom Reference   --即将释放

### 1.3 四种GC
    Serial GC
        单线程GC,工作时暂停所有应用的线程,STW(stop the world)
    Parallel GC
        多个并行的GC线程.默认GC.工作时也会暂停所有应用的线程,STW
    CMS GC
        Concurrent Mark Sweep
        并发标记-清除GC.
        使用更多CPU来保证应用的性能.
        压缩时STW.
    G1 GC
        针对较大的堆内存空间
        将堆分成多个区域,并行管理,随时在释放之后压缩空闲堆空间

### 1.4 GC监控与分析

    为了便于直观地监控Java应用中的堆的情况,cpu占用情况,以及GC中的分代情况等,Oracle提供了多种工具,比如Java VisualVM.

## 2.阅读 HotSpot JVM 源码!

### 2.1 概览
#### 2.1.1 JDK9的GC新特性:

    JDK9除了模块化系统以及JShell亮点之外,GC变化也很大:
    默认虚拟机HotSpot的默认GC从之前的Parallel GC更改为G1 GC.
    彻底去除了CMS的foreground模式,增量CMS,开始弃用(deprecate)剩下的CMS.
#### 2.1.2 HotSpot架构:

    获取源码之后得到如下图所示目录结构的jdk9的HotSpot源码:
![01_概览][2]

    HotSpot项目主体主要是用C++实现的,并伴有少量的C代码和汇编代码(平台相关).
    当然也有java的部分,负责调试的SA(Serviceability Agent)部分由Agent目录里的java实现.
    share目录里面是平台无关的共享代码,我们关注的gc以及memory模块,就在里面.
    JDK9 HotSpot中,方发区(metaspace)和堆被所有线程共享,栈和指令计数器为现成所私有.内存的自动管理及回收在堆中进行.
### 2.2 GC的初始化
在memory/universe中做初始化准备

    memory模块主要有universe, metaspace, heap等.
    metaspace模块是memory中代码量最大的模块,属于非堆区,前身是jdk8之前PermGen的方法区.
    由于我们关注的是自动内存管理部分,所以只关注gc相关的部分.
    与gc关联较大的就是universe子模块.
    在universe内部开始gc的初始化准备工作
    universe是memory模块中的命名空间.其中包含着复杂的vm已知类与对象,以及各种堆相关,内存错误相关,指针长度相关,调试相关的函数等.
    另外,universe中也实现了initialize_heap等开始初始化gc的工作.

![02_universe][3]

    universe::initialize_heap():
        jvm堆的初始化,调用create_heap()创建heap,
        并初始化_collectedHeap,再初始化TLAB等
![03_initialize_heap][4]

    universe::create_heap():
        创建heap,通过create_heap_with_policy
        <G1CollectedHeap, G1CollectorPolicy>()等设置gc类别
        可以看出,CMS和SerialGC用的CollectedHeap类型
        都是GenCollectedHeap.
        
![04_create_heap][5]

        create_heap_with_policy中创建GC Policy对象,GC Heap对象
        
![06_create_heap_with_policy][6]

    universe::universe_post_init():
        另外值得一提的是,universe中代码行数最多(180+)的method,主要负责初始化后的一些操作,
        如加载异常等基础类,构建各种错误信息,安全检查等.
        在runtime::init时调用.
![05_universe_post_init][7]
    
### 2.3 GC-Shared部分
shared部分是不同gc所共享的代码.
#### 2.3.0 CollectedHeap

    位于gc/shared/collectedHeap中
    是为了实现不同类型GC的heap而构造的抽象类,
    包含了一个heap所必需的函数和组件.
    


#### 2.3.1 GenCollectedHeap

    位于gc/shared/genCollectedHeap中.
    genCollectedHeap是分代的CollectedHeap.
    分为young generation和old generatioin.
    
#### 2.3.2 CollectorPolicy

    位于gc/shared/collectorPolicy中.
    

### 2.4 Serial GC

    Serail GC的Heap为GenCollectedHeap,位于gc/shared/genCollectedHeap中.
    Serail GC的Policy为MarkSweepPolicy,位于gc/shared/collectorPolicy中.






​    
### **杂记-魔法**:

    宏-##:
        #define DEF_OOP(type)                   \
            class type##OopDesc;                \
                class type##Oop : public oop {  \
                public:                         \
                    type##Oop() : oop() {}      \
                    operator type##OopDesc* () const { return (type##OopDesc*)obj(); }  \
            };
    宏-循环:
        #define ALL_JAVA_THREADS(x) for (JavaThread* x=_thread_list; x; x = x->next();
        ALL_JAVA_THREADS(p){tc->do_thread(p);}

### **杂记-心得**:

    1.阅读源码:
        阅读源码,是为了理解源码的工作原理,了解各个组件如何协作发挥作用.而源码的"数据结构"最能直接反映组件的本质,反映依赖关系,反映状态条件.   
    2.在查找某个函数或变量在哪里使用的时候,可以用
        for i in $(ls); do echo $i; cat -n $i |grep G1CollectedHeap; done
        的方法查找,高效方便.
    3.开始可能会走很多弯路,比如不清楚某个模块,深入了解之后发现,跟自己想要探究的东西相关性很小.
    
### links:
1   [Java垃圾回收的介绍](https://javapapers.com/java/java-garbage-collection-introduction/)    
2   [Java垃圾回收的工作机制](https://javapapers.com/java/how-java-garbage-collection-works/)
3   [Java垃圾回收的类型](https://javapapers.com/java/types-of-java-garbage-collectors/)  
4   [Java垃圾回收的监测与分析](https://javapapers.com/java/java-garbage-collection-monitoring-and-analysis/)  
5   [Visualvm-可视化VM](https://docs.oracle.com/javase/8/docs/technotes/guides/visualvm/)  
6   [HotSpot实战](https://book.douban.com/subject/25847620/)  
7   [Java堆的创建](http://www.importnew.com/17068.html)   
8   [从持久代到metaspace](https://juejin.im/post/59e969ca51882561a05a3340)  
9   [metaspace in java8](http://java-latte.blogspot.sg/2014/03/metaspace-in-java-8.html)  

[1]: https://javapapers.com/java/how-java-garbage-collection-works/
[2]: http://home.ustc.edu.cn/~jzw0222/01_%E6%A6%82%E8%A7%88.png
[3]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/02_universe.png
[4]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/03_initialize_heap.png
[5]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/04_create_heap.png
[6]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/06_create_heap_with_policy.png
[7]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/05_universe_post_init.png

