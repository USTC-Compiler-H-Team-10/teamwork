# Notes for [Java Garbage Collection](https://javapapers.com/java/how-java-garbage-collection-works/)

##1.初步认识 JVM

#### 1.1 分代GC
    eden space(伊甸园空间)
    Survivor Space(幸存空间) S0和S1
    Old Generation(年老代)
    (SE 8中移除了"永久代")
#### 1.2 四种引用
    Strong Reference    --不释放
    Soft Reference      --可以释放,但是尽可能作为最后一个释放选项
    Weak Reference      --即将释放
    Phantom Reference   --即将释放

#### 1.3 四种GC
    Serial GC
        单线程GC,工作时暂停所有应用的线程,STW(stop the world)
    Parallel GC
        多个并行的GC线程.默认GC.工作时也会暂停所有应用的线程,STW
    CMS GC
        Concurrent Mark Sweep
        并发标记-清除GC.
        使用更多CPU来保证应用的性能.
        如果cpu资源充足,CMS是首选.
        压缩时STW.
    G1 GC
        针对较大的堆内存空间
        将堆分成多个区域,并行管理,随时在释放之后压缩空闲堆空间

#### 1.4 GC监控与分析

    为了便于直观地监控Java应用中的堆的情况,cpu占用情况,以及GC中的分代情况等,Oracle提供了多种工具,比如Java VisualVM.


#### links:

    1   https://javapapers.com/java/java-garbage-collection-introduction/
    2   https://javapapers.com/java/how-java-garbage-collection-works/
    3   https://javapapers.com/core-java/java-weak-reference/
    4   https://javapapers.com/java/types-of-java-garbage-collectors/
    5   https://javapapers.com/java/java-garbage-collection-monitoring-and-analysis/
    6   https://docs.oracle.com/javase/8/docs/technotes/guides/visualvm/
