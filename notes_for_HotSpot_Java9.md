# Notes for [Java Garbage Collection][1]

## 1.初步认识 JVM

### 1.1 分代GC
    eden space(伊甸园空间)
    Survivor Space(幸存空间) S0和S1
    Old Generation(年老代)
    ("永久代"是HotSpot虚拟机的特有概念,并非jvm规范中定义.并且从java8开始,HotSpot去除了"永久代")

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
        STW时间很短.
### 1.4 GC监控与分析

    为了便于直观地监控Java应用中的堆的情况,cpu占用情况,以及GC中的分代情况等,Oracle提供了多种工具,比如Java VisualVM.

## 2.阅读 HotSpot JVM 源码!

### 2.1 概览
#### 2.1.1 java9的GC新特性:

    java9除了模块化系统以及JShell亮点之外,GC变化也很大:
    默认虚拟机HotSpot的默认GC从之前的Parallel GC更改为G1 GC.
    彻底去除了CMS的foreground模式,增量CMS,开始弃用(deprecate)剩下的CMS.
#### 2.1.2 HotSpot架构:

    获取源码之后得到如下图所示目录结构的java9的HotSpot源码:
![01_概览][2]

    HotSpot项目主体主要是用C++实现的,并伴有少量的C代码和汇编代码(平台相关).
    当然也有java的部分,负责调试的SA(Serviceability Agent)部分由Agent目录里的java实现.
    share目录里面是平台无关的共享代码,我们关注的gc以及memory模块,就在里面.
    java9 HotSpot中,方发区(metaspace)和堆被所有线程共享,栈和指令计数器为现成所私有.内存的自动管理及回收在堆中进行.
### 2.2 GC的初始化
在memory/universe中做初始化准备

    memory模块主要有universe, metaspace, heap等.
    metaspace模块是memory中代码量最大的模块,属于非堆区,前身是java8之前PermGen的方法区.
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
#### 2.3.1 CollectedHeap

    位于gc/shared/collectedHeap中
    是为了实现不同类型GC的heap而构造的抽象类,
    包含了一个heap所必需的函数和功能,比如创建新TLAB,内存分配等基本功呢个呢.
    GenCollectedHeap,G1CollectedHeap,ParallelScavengeHeap等都是其子类.
    
    class GCHeapLog:
        用来记录GC的事件.
    CollectedHeap::pre_initialize():
        所有GCHeap初始化前的必需的准备.
    CollectedHeap::mem_allocate():
        虚方法,内存分配,不适用于TLAB,失败则抛出OutOfMemory异常    
    CollectedHeap::allocate_new_tlab():
        虚方法,创建一个新的TLAB,所有TLAB的创建都用这个method.
    CollectedHeap::allocate_from_tlab():
        从当前线程的TLAB分配内存空间.
    CollectedHeap::post_initialize():
        虚方法,在general heap allocation之前,
        进行的标志位的设置等初始化工作,
        实际上是在内部调用policy的post_heap_initialize方法.
    CollectedHeap::stop():
        虚方法,停止所有并发的工作.
    CollectedHeap::top_addr(),end_addr():
        虚方法,返回边界
    
#### 2.3.2 GCCause

    位于gc/shared/gcCause中,定义了引起GC的原因类型.
    
    GCCause::Cause():
        枚举类型,枚举了引起GC的各种原因.
    GCCause::is_user_requested_gc():
        返回用户是否发出gc请求
    GCCause::is_serviceability_requested_gc():
        返回是否debug时中发出.
    GCCause::is_tenured_allocation_failure_gc():
        返回是否OldGen分配失败引起.
    GCCause::is_allocation_failure_gc():
        返回是否分配失败引起.
    GCCause::to_string():
        返回cause的字符串表示.
#### 2.3.3 CollectorPolicy

    位于gc/shared/collectorPolicy中.
    用于定义gc的全局属性,包括在初始化的过程中需要的和其他公共需求.
    这个类还没有充分开发,后续会在加入新的gc时加入更多的公共功能.
    
    CollectorPolicy::initialize_flags():
        初始化过程中,所有gc的flag设置以及验证都在这里实现.
    CollectorPolicy::initialize_size_info():
        初始化size信息
    CollectorPolicy::mem_allocate_work():
        虚方法,子类所要实现的,对对象分配内存空间的method.
        
    GenCollectorPolicy和MarkSweepPolicy也在相同的源文件,
    分别实现自己的method.
    
#### 2.3.4 GenCollectorPolicy    

    GenCollectorPolicy::mem_allocate_work():
        分配制定大小的空间,根据参数is_tlab来判断分配空间的来源,是内存堆还是TLAB.
![11_1][12]
![11_2][13]
![11_3][14]

    其中,值得说明的点:
    596行尝试分配时,是先尝试youngGen,
    如果尝试失败,并且first_only是false,就再尝试oldGen.
    611行尝试可扩展内存时,是先尝试扩展oldGen的空间.
    最后,
    650,651行可以看出,触发gc的操作并不是由自身执行gc,
    而是通过JVM操作指令执行,GenCollectorPolicy只负责监听是否完成gc即可.
    下面是VMThread::execute()
![12_VMThread_execute_notify][15]

    在虚拟机创建时就会创建一个单例原生线程VMThread。这个线程能派生出其他线程。同时，这个线程的主要的作用是维护一个vm操作队列(VMOperationQueue)，用于处理其他线程提交的vm operation，比如执行GC等。
#### 2.3.5 MarkSweepPolicy

    GenCollectorPolicy的子类.
    MarkSweepPolicy的流程:
        标记active对象,清理非active对象,
        尽量把gc的范围控制在youngGen,因为清理youngGen相对容易,
        只需要把eden/from(也就是s0)中的active对象复制到to(也就是s1)/oldGen,
        然后再清除整个eden/from即可.
        而清理oldGen,没有其他区可以复制过去,所以只能在oldGen内部通过移动压缩的方法腾出空间,而这样过程比较复杂.
        
        
#### 2.3.6 GenCollectedHeap

    位于gc/shared/genCollectedHeap中.
    genCollectedHeap是分代的CollectedHeap.
    分为young generation和old generatioin.
    GenCollectedHeap除了负责java对象的内存分配,还负责垃圾回收,
    依赖CollectorPolicy.

    初始化:
    GenCollectedHeap::initialize():
        初始化内存堆,内存堆的数量由policy决定.
![07_genCollectedHeap_初始化][8]

    申请内存:
    GenCollectedHeap::allocate():
        向os申请内存.
![08_genCollectedHeap_申请空间][9]       

    分配内存:
    GenCollectedHeap::mem_allocate():
        内存堆中分配指定大小的内存块,不适用于TLAB,
        失败则抛出OutOfMemory异常.
    GenCollectedHeap::allocate_new_tlab():
        为某一线程申请一块本地分配缓冲区,
        所有TLAB的分配都用这个method.
    这两者都是通过
    调用collector_policy()->mem_allocate_work()方法,
    只是参数不同.
![10_genCollectedHeap_mem_allocate][11]

![09_genCollectedHeap_allocate_new_tlab][10]
### 2.4 Serial GC

    Serail GC的Heap为GenCollectedHeap.
    Serail GC的Policy为MarkSweepPolicy.






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
[8]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/07_genCollectedHeap_%E5%88%9D%E5%A7%8B%E5%8C%96.png
[9]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/08_genCollectedHeap_%E7%94%B3%E8%AF%B7%E7%A9%BA%E9%97%B4.png
[10]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/09_genCollectedHeap_allocate_new_tlab.png
[11]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/10_genCollectedHeap_mem_allocate.png
[12]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/11_collectorPolicy1.png
[13]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/11_collectorPolicy2.png
[14]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/11_collectorPolicy3.png
[15]: https://raw.githubusercontent.com/leo2589/USTC/master/tmp/pic/12_VMThread_execute_notify.png