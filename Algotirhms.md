#三个算法：GC复制法，标记压缩，增量式垃圾回收
###GC复制算法
####V1：1969 Robert R. Fenichel 和 Jerome C. Yochelson
Robert R. Fenichel and Jerome C. Yochelson, A LISP garbage-collector for vitual-memory computer systems, Communications of the ACM, v.12 n.11, p.611-612, Nov 1969
**核心思想**: 把堆空间一分为二，分别当作正在使用的From空间和To空间。当我们正在使用的From空间满了之后，执行GC复制算法，从From空间把还活着的对象全部复制的To空间，这样子那些死了的对象就不会被复制，此时To空间包含了From中的活对象，而且有部分空间被释放了出来。然后把To空间当成我们现在正在新使用From空间继续使用。
**算法**：
```
核心算法：
copying(){
	$free = $to_start
	for( r  : $roots)
		*r = copy(*r)
		
	swap($from_start, $to_start)
}
其中copy函数：
copy(obj){
	if(obj.tag != COPIED)
		copy_data($free, obj, obj.size)
		obj.tag = COPIED
		obj.forward = $free
		$free+=obj.size
	
	for(child : children(obj.forwarding))
		*child = copy(*child)
	
	return obj.forwarding
}
```
**优点**
1. 吞吐量优秀。与GC标记-清除算法相比，GC复制算法的速度很快。
2. 可以实现高速分配。GC复制算法不需要空闲区域链表。因为我们能用的那个内存分块是一块连续的内存空间，所以只要分块大小不小于申请的大小，直接移动$free指针就可以完成分配。
3. 不会发生碎片化。每次GC完毕，对象都集中在内存的一头，即每次GC复制算法结束后都会实现压缩。
4. 与缓存兼容。内存中连续的对象是父子关系。这样，cache能够得到高效利用。
**缺点**
1. 堆的使用效率低下。很明显的，我们只能用整个堆空间的一半大小。
2. 不兼容保守式GC。GC复制法必须要移动对象重写指针。
3. 递归调用函数会导致额外负担非常大。
####V2：1970 Cheney 的 GC复制算法
