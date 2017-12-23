#三个算法：GC复制法，标记压缩，增量式垃圾回收

###GC复制算法
####V1：1969 Robert R. Fenichel 和 Jerome C. Yochelson的GC复制算法
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
4. 与缓存兼容。内存中连续的对象是父子关系（我想说引用关系。在引用的树上，他们是父子。）。这样，cache能够得到高效利用。
**缺点**
1. 堆的使用效率低下。很明显的，我们只能用整个堆空间的一半大小。
2. 不兼容保守式GC。GC复制法必须要移动对象重写指针。
3. 递归调用函数会导致额外负担非常大。
####V2：1970 Cheney 的 GC复制算法
C.J. Cheney, A nonrecursive list compacting algorithm, Communications of the ACM, v.13 n.11, p677-678, Nov. 1970
**核心**
针对Fenichel和Yochelson的GC复制算法的**复制算法递归调用**这一问题，额外负担大，消耗栈且可能爆栈的缺点，Cheney使用广度有限搜索来进行GC复制算法，解决了递归调用的问题，但实际上，也带来了其它问题。
**算法**
```
copying(){
	scan = $free = $to_start
	for(r : roots)
		*r = copy(*r)
	while(scan!=free)
		for(child : children(scan))
			*child = copy(*child)
		scan += scan.size
	swap($from_start, $to_start)
}
其中，copy函数：
copy(obj){
	if(is_pointer_to_heap(obj.forward, $to_start) == FALSE)
		copy_data($free, obj, obj.size)
		obj.forwarding = $free
		$free += obj.size
	return obj.forwarding
}
```
**优点**
把scan和$free之间的堆变成了队列，从而不构建队列而完成了FIFO的广度有限搜索，省去了用于搜索的内存空间。而且解决了F和Y的GC复制算法的递归问题。
**缺点**
广度有限搜索导致在To空间中，相邻的对象是兄弟关系而不是父子关系，这样与cache缓存不兼容哦。因为我们知道，如果我们在处理一个obj，我们继续访问obj.child的可能性是非常大的。
####V3近似深度有限搜索1991 Paul R. Wilson, MIchael S. Lam，Thomas G. Moher
**核心思想**
针对Cheney的GC复制算法中**对cache缓存不兼容**的问题，进行改良产生了近似优先搜索。这个算法采取的不是完整的广度优先算法，而是在cache中每个page上分别进行广度有限搜索。这里利用了广度有限搜索的性质，搜索一开始把有引用关系的对象安排在同一个页面。
####V4多空间复制算法。
**核心思想**
GC复制算法最大的问题是只能利用半个堆。那么如果我们把堆分成N块，只把其中的一块当作To区域，那么我们就可以利用很大块的堆了。对其中两块当作From和To，进行GC复制算法，剩下的N-2块进行标记-清除算法。然后把From和To向后挪。把这两种算法结合起来来用。
**算法**
```
multi_space_copying(){
	$free = $heap[$to_space_index]
	for(r : $roots)
		*r = mark_or_copy(*r)
	for(index : 0..(N-1))
		if(is_copying_index(index)==FALSE)
			sweep_block(index)
	$to_space_index = $from_space_index
	$from_space_index = ($from_space_index + 1) % N
}

mark_or_copy(obj){
	if(is_pointer_to_from_space(obj) == TREE)
		return copy(obj)
	else
		if(obj.mark==FALSE)
		obj.mark = TRUE
		for(child : children(obj)
			*child = mark_or_copy(*child)
		return obj
}

copy(obj){
if(obj.tag==COPIED)
	copy_data($free, obj, obj.size)
	obj.tag = COPIED
	obj.forwarding = $free
	$free += obj.size
	for(child : children(obj.forwarding)
		*child = mark_or_copy(*child)
	return obj.forwarding
}
	
```
**优点**
对堆空间的利用率提升，只有1/N的堆空间是空的，剩下爱的都可以用
**缺点**
emmm。是GC标记-清除法和GC复制法的结合，所以……优缺点也就是他们俩的优缺点。
###GC标记-压缩法
####V1 Lisp2算法
**核心思想**
把堆进行压缩，把所有活的对象都放放在堆的一边，空闲的内存空间拼接在一起在堆的另一端。这样的好处是非常显而易见的。过程分为三步，需要扫描整个堆空间三次，因此会比其他的GC算法都要慢。
标记部分和GC标记-清除算法中的标记一样。所以不再写。
压缩部分：
```
三个步骤：
compaction_phase(){
	set_forwarding_ptr()
	adjust_ptr()
	move_obj()
}
```
第一步是设定forwarding指针。一开始全都设置成NULL。扫描完成后，把活对象的forwarding指向压缩后那个对象应该在的位置。
```
set_forwarding_ptr(){
	scan = new_address = $heap_start
	while(scan < $heap_end)
		if(scan_mark = true)
			scan.forwarding = new_address
			new_address += scan.size
		scan += scan.size
```
第二步是更新各个对象的指针。
```
adjust_ptr(){
	for(r:roots)
		*r = (*r).forwarding
	
	scan = $heap_start
	while(scan < heap_end)
		if(scan.mark == TRUE)
			for(child : children(scan))
				*child = (*child).forwarding
		scan += scan.size
}
```
第三步是再次遍历整个堆，把活的对象移动到forwarding的该在的位置。
```
move_obj(){
	scan = $free = $heap_start
	while(scan < $heap_end)
		if(scan..mark = TRUE)
			new_address = scan.forwarding
			copy_data(new_address, scan, scan.size)
			new_address.forwarding = NULL
			new_address.mark = FLASE
			$free += new_address.size
			scan += scan,size
}
```
**优点**
1. 堆利用率高
2. 不会产生碎片化
**缺点**
花费时间太长。需要扫描整个堆三遍。因此吞吐量很低。
需要给每一个对象补充额外的一个域forwarding来记录移动后的位置。
####V2 1964年Saunders 的Two-FInger压缩算法
Robert A. Saunders, The LISP system for the Q-32 computer. In THe Programming Language LISP: ITs Operation and Applications, Berkeley, E.C. and Bobrow, D.G., Eds., Infomation International, Cambridge, Mass., p.220-231, 1964
**核心思路**
这个算法有一个很强的前提，就是在着一块堆中的所有单元要一样大。就是说，里面的无论是死的还是活得对象或者空闲块都必须要一样大（空闲块可以是若干个一样大的和）。针对Lisp2算法中，需要扫描三遍吞吐量小，以及需要始终保存每个活对象的Forwarding域的问题，Two-Finget算法给予解决。虽然它要求所有的对象的大小都一样这个前提太严格，但是在GC标记-删除法中，BiBOP方法，把统一大小的对象安排在同一个分块中，就可以使用Two-finger算法。
**算法**
解决的方法很朴素，就是说，因为对象的大小都一样了，所以只遍历两遍堆。第一遍移动，把活对象搬到前面的空闲块。第二遍更改指针，指向现在的新的对象的位置。
**优点**
就算核心思路中解决的Lisp2的几个缺点。
**缺点**
1. 可以看到，搬运的过程中没有考虑过引用的父子关系，这样，对cache缓存就不适合。
2. 前提限制条件太严格，需要所有对象一样大。但是在BiBOP的前提下可以解决。
####V3 1967年B.K. Haddon和W.M. Waite的表格算法
**核心想法**
在Lisp2算法中，因为要记录移动后的位置，所以要给每个对象补充额外的forwarding域；而Two-finger算法中对这两个问题的解决并不好。因为限定了大小，而且对象之间的关系被打乱了，对cache很不友好。所以，表格算法利用空闲的块来记录所需要的信息，并在GC之后放弃这些块，从而不需要额外的域，而且保存了对象之间的引用关系。
**算法**
1. 移动对象群
```
move_obj(){
	scan = $free = $heap_start
	size = 0
	while(scan < $heap_end)
		while(scan.mark == FALSE)
			size +=scan.size
			scan += scan.size
		live = scan
		while(scan.mark == TRUE)
			scan +=scan.size
		slide_objs_and_make_bt(scan, $free, live, size)
		$free += (scan - live)
}
其中，slide_objs_and_make_bt()是在构建间隙表格和移动间隙表格。
slide_objs_and_make_bt(){
	在一块对象群后创建间隙表格
	间隙表格记录对象群的首地址和左边空闲分块大小
	如果后面搬运的对象群要盖住这个间隙表格，那么让所有已有对象表格移动到后面空闲的地方去，然后做相同的操作
}

2. 更新指针
adjust_ptr(){
	for( r: roots)
		*r = new address(*r)
	scan = $heap_start
	while(scan < $free)
		scan.mark = FALSE
		for(child : children(*child))
			*child = new_address(*child)
		scan +=scan.size
}
其中，new_address()是为了得到每个对象的移动后的位置，去查询间隙表格，那么它的所在的对象群的间隙表格中，原来的起始位置-左边的空内存的大小就等于向左移动了的距离数。
new_address(obj){
	best_entry = new_bt_entry(0,0)
	for(enter : break_table)
		if(entry.address <= obj && $best_entry.address <entry.address)
			best_entry = entry
	return obj-best_entry.size
}
	
```
**优点**
1. 不需要额外的forwarding域
2. 维护了对象之间的位置关系，对cache友好。
**缺点**
维持间隙表格需要很高的代价。
###增量式垃圾回收
