
##1. 原有的基于TSS的任务切换的不足
---
>原有的Linux 0.11采用基于TSS和一条指令，虽然简单，但这指令的执行时间却很长，在实现任务切换时大概需要200多个时钟周期。而通过堆栈实现任务切换可能要快，而且采用堆栈的切换还可以使用指令流水的并行化优化技术，同时又使得CPU的设计变得简单。所以无论是Linux还是Windows，进程/线程的切换都没有使用Intel 提供的这种TSS切换手段，而都是通过堆栈实现的。

##2. 进程切换的六段论
---
基于内核栈实现进程切换的基本思路：当进程由用户态进入内核时，会引起堆栈切换，用户态的信息会压入到内核栈中，包括此时用户态执行的指令序列EIP。由于某种原因，该进程变为阻塞态，让出CPU，重新引起调度时，操作系统会找到新的进程的PCB，并完成该进程与新进程PCB的切换。如果我们将内核栈和PCB关联起来，让操作系统在进行PCB切换时，也完成内核栈的切换，那么当中断返回时，执行`IRET`指令时，弹出的就是新进程的EIP，从而跳转到新进程的用户态指令序列执行，也就完成了进程的切换。**这个切换的核心是构建出内核栈的样子，要在适当的地方压入适当的返回地址，并根据内核栈的样子，编写相应的汇编代码，精细地完成内核栈的入栈和出栈操作，在适当的地方弹出适当的返回地址，以保证能顺利完成进程的切换。同时完成内核栈和PCB的关联，在PCB切换时，完成内核栈的切换。**
___
###2.1 中断进入内核
-  **为什么要进入内核中去？**  
大家都知道，***操作系统***负责进程的调度与切换，所以进程的切换一定是在***内核***中发生的。要实现进程切换，首先就要进入内核。而用户程序都是运行在用户态的，在Linux中，应用程序访问内核唯一的方法就是***系统调用***，应用程序通过操作系统提供的若干系统调用函数访问内核，而该进程在内核中运行时，可能因为要访问磁盘文件或者由于时间片耗完而变为阻塞态，从而引起调度，让出CPU的使用权。
-  **从用户态进入内核态，要发生堆栈的切换**
系统调用的核心是指令` int 0x80 `这个系统调用中断。一个进程在执行时，会有函数间的调用和变量的存储，而这些都是依靠堆栈完成的。进程在用户态运行时有用户栈，在内核态运行时有内核栈，所以当执行系统调用中断` int 0x80 `从用户态进入内核态时，一定会发生栈的切换。而这里就不得不提到TSS的一个重要作用了。进程内核栈在线性地址空间中的地址是由该任务的TSS段中的ss0和esp0两个字段指定的，依靠TR寄存器就可以找到当前进程的TSS。也就是说，**当从用户态进入内核态时，CPU会自动依靠TR寄存器找到当前进程的TSS，然后根据里面ss0和esp0的值找到内核栈的位置，完成用户栈到内核栈的切换。TSS是沟通用户栈和内核栈的关键桥梁，这一点在改写成基于内核栈切换的进程切换中相当重要！**
- **从用户态进入内核发生了什么？**
当执行` int 0x80` 这条语句时由用户态进入内核态时，CPU会自动按照***SS、ESP、EFLAGS、CS、EIP***的顺序，将这几个寄存器的值压入到内核栈中，由于执行` int 0x80 `时还未进入内核，所以压入内核栈的这五个寄存器的值是用户态时的值，其中***EIP***为` int 0x80 `的下一条语句 `"=a" (__res) `，这条语句的含义是**将eax所代表的寄存器的值放入到_res变量中。所以当应用程序在内核中返回时，会继续执行 "=a" (__res) 这条语句。**这个过程完成了进程切换中的第一步，**通过在内核栈中压入用户栈的ss、esp建立了用户栈和内核栈的联系，形象点说，即在用户栈和内核栈之间拉了一条线，形成了一套栈。**
-	**内核栈的具体样子**
 父进程内核栈的样子
执行`int 0x80`将SS、ESP、EFLAGS、CS、EIP入栈。
在system_call中将DS、ES、FS、EDX、ECX、EBX入栈。

```
	system_call:
		cmpl $nr_system_calls-1,%eax
		ja bad_sys_call
		push %ds
		push %es
		push %fs
		pushl %edx
		pushl %ecx		# push %ebx,%ecx,%edx as parameters
		pushl %ebx		# to the system call
		movl $0x10,%edx		# set up ds,es to kernel space
		mov %dx,%ds
		mov %dx,%es
		movl $0x17,%edx		# fs points to local data space
		mov %dx,%fs
		call sys_call_table(,%eax,4)
		pushl %eax
		movl current,%eax
		cmpl $0,state(%eax)		# state
		jne reschedule
		cmpl $0,counter(%eax)		# counter
		je reschedule
```
在*system_call*中执行完相应的系统调用*sys_call_xx*后，又将函数的返回值eax压栈。若引起调度，则跳转执行*reschedule*。否则则执行*ret_from_sys_call*。
```
reschedule:
	pushl $ret_from_sys_call
	jmp schedule
```
在执行*schedule*前将*ret_from_sys_call*压栈，因为*schedule*是c函数，所以在c函数末尾的`}`，相当于`ret`指令，将会弹出*ret_from_sys_call*作为返回地址，跳转到*ret_from_sys_call*执行。
总之，在系统调用结束后，将要中断返回前，内核栈的样子如下：

内核栈	|
---	|
SS	|
ESP	|
EFLAGS	|
CS	|
EIP	|
DS	|
ES	|
FS	|
EDX	|
ECX	|
EBX	|
EAX	|
ret_from_sys_call	|
___

###2.2 找到当前进程的PCB和新进程的PCB
-  当前进程的PCB
当前进程的PCB是用一个全局变量current指向的*(在sched.c中定义)* ，所以current即指向当前进程的PCB
-  新进程的PCB
为了得到新进程的PCB，我们需要对schedule()函数做如下修改：
```c
void schedule(void)
{
	int i,next,c;
	struct task_struct *pnext = &(init_task.task);
	struct task_struct ** p;    /* add */
	......
	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter,next = i,pnext=*p;
		}    /* edit */
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
			switch_to(pnext,_LDT(next));    /* edit */
}
```
这样，pnext就指向下个进程的PCB。
在*schedule()*函数中，当调用函数*switch_to(pent, _LDT(next))*时，会依次将返回地址***}***、参数2 ***_LDT(next)***、参数1 ***pnext***压栈。当执行*switch_to*的返回指令`ret`时，就回弹出*schedule()*函数的***}***执行*schedule()*函数的返回指令`}`。关于执行*switch_to*时内核栈的样子，在后面改写*switch_to*函数时十分重要。
此处将跳入到*switch_to*中执行时，内核栈的样子如下：

内核栈	|
---	|
SS	|
ESP	|
EFLAGA	|
CS	|
EIP	|
DS	|
ES	|
FS	|
EDX	|
ECX	|
EBX	|
EAX	|
ret_from_sys_call	|
pnext	|
_LDT(next)	|
}	|
___
### 2.3 完成PCB的切换
### 2.4 根据PCB完成内核栈的切换
### 2.5 切换运行资源LDT
这些工作都将有改写后的*switch_to*完成。
> 将Linux 0.11中原有的switch_to实现去掉，写成一段基于堆栈切换的代码。由于要对内核栈进行精细的操作，所以需要用汇编代码来实现switch_to的编写，**既然要用汇编来实现switch_to，那么将switch_to的实现放在system_call.s中是最合适的。**这个函数依次主要完成如下功能：由于是c语言调用汇编，所以需要首先在汇编中处理栈帧，即处理ebp寄存器；接下来要取出表示下一个进程PCB的参数，并和current做一个比较，如果等于current，则什么也不用做；如果不等于current，就开始进程切换，依次完成PCB的切换、TSS中的内核栈指针的重写、内核栈的切换、LDT的切换以及PC指针（即CS:EIP）的切换。

switch_to(system_call.s)的基本框架如下：
```
switch_to:
	pushl %ebp
	movl %esp,%ebp
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl 8(%ebp),%ebx
	cmpl %ebx,current
	je 1f
	切换PCB
	TSS中的内核栈指针的重写
	切换内核栈
	切换LDT
	movl $0x17,%ecx
	mov %cx,%fs
	cmpl %eax,last_task_used_math    //和后面的cuts配合来处理协处理器，由于和主题关系不大，此处不做论述
	jne 1f
	clts
1:	popl %eax
	popl %ebx
	popl %ecx
	popl %ebp
	ret
```
理解上述代码的核心，是理解栈帧结构和函数调用时控制转移权方式。
> 大多数CPU上的程序实现使用栈来支持函数调用操作。栈被用来传递函数参数、存储返回地址、临时保存寄存器原有值以备恢复以及用来存储局部数据。单个函数调用操作所使用的栈部分被称为**栈帧**结构，其通常结构如下：

<div  align="center"> <img src="http://img.blog.csdn.net/20150629012458362" width = "300" height = "500" alt="图片名称" align=center /> </div>

栈帧结构的两端由两个指针来指定。寄存器ebp通常用作帧指针，而esp则用作栈指针。在函数执行过程中，栈指针esp会随着数据的入栈和出栈而移动，因此函数中对大部分数据的访问都基于帧指针ebp进行。
对于函数A调用函数B的情况，传递给B的参数包含在A的栈帧中。当A调用B时，函数A的返回地址（调用返回后继续执行的指令地址）被压入栈中，栈中该位置也明确指明了A栈帧的结束处。而B的栈帧则从随后的栈部分开始，即图中保存帧指针(ebp)的地方开始。再随后则用来存放任何保存的寄存器值以及函数的临时值。

所以执行完指令`pushl %eax`后，内核栈的样子如下：

<div  align="center"> <img src="http://img.blog.csdn.net/20150629012549623" width = "300" height = "500" alt="图片名称" align=center /> </div>

switch_to中指令`movl 8(%ebp),%ebx`即取出参数2_LDT(next)放入寄存器ebx中，而12(%ebp)则是指参数1penxt。

-  完成PCB的切换

```
movl %ebx,%eax
xchgl %eax,current
```

-  TSS中的内核栈指针的重写
如前所述，当从用户态进入内核态时，CPU会自动依靠TR寄存器找到当前进程的TSS，然后根据里面ss0和esp0的值找到内核栈的位置，完成用户栈到内核栈的切换。所以仍需要有一个当前TSS，我们需要在schedule.c中定义`struct tss_struct *tss=&(init_task.task.tss)`这样一个全局变量，即0号进程的tss，所有进程都共用这个tss，任务切换时不再发生变化。
虽然所有进程共用一个tss，但不同进程的内核栈是不同的，所以在每次进程切换时，需要更新tss中esp0的值，让它指向新的进程的内核栈，并且要指向新的进程的内核栈的栈底，即要保证此时的内核栈是个空栈，帧指针和栈指针都指向内核栈的栈底。
这是因为新进程每次中断进入内核时，其内核栈应该是一个空栈。为此我们还需要定义：`ESP0 = 4`，这是TSS中内核栈指针esp0的偏移值，以便可以找到esp0。具体实现代码如下：

```
movl tss,%ecx
addl $4096,%ebx
movl %ebx,ESP0(%ecx)
```

-  内核栈的切换
> Linux 0.11的PCB定义中没有保存内核栈指针这个域(kernelstack)，所以需要加上，而宏KERNEL_STACK就是你加的那个位置的偏移值，当然将kernelstack域加在task_struct中的哪个位置都可以，但是在某些汇编文件中（主要是在system_call.s中）有些关于操作这个结构一些汇编硬编码，所以一旦增加了kernelstack，这些硬编码需要跟着修改，由于第一个位置，即long state出现的汇编硬编码很多，所以kernelstack千万不要放置在task_struct中的第一个位置，当放在其他位置时，修改system_call.s中的那些硬编码就可以了。

在schedule.h中将struct task_struct修改如下：

```
struct task_struct {
long state;
long counter;
long priority;
long kernelstack;
......
}
```

同时在system_call.s中定义`KERNEL_STACK = 12`
并且修改汇编硬编码，修改代码如下：

```
ESP0		= 4
KERNEL_STACK	= 12

......

state	= 0		# these are offsets into the task-struct.
counter	= 4
priority = 8
kernelstack = 12
signal	= 16
sigaction = 20		# MUST be 16 (=len of sigaction)
blocked = (37*16)
```

switch_to中的实现代码如下：

```
movl %esp,KERNEL_STACK(%eax)
movl 8(%ebp),%ebx
movl KERNEL_STACK(%ebx),%esp
```

由于这里将PCB结构体的定义改变了，所以在产生0号进程的PCB初始化时也要跟着一起变化，需要在schedule.h中做如下修改：

```
#define INIT_TASK \
/* state etc */	{ 0,15,15,PAGE_SIZE+(long)&init_task,\
/* signals */	0,{{},},0, \
......
}
```

-  LDT的切换
switch_to中实现代码如下：

```
movl 12(%ebp),%ecx
lldt %cx
```

一旦修改完成，下一个进程在执行用户态程序时使用的映射表就是自己的LDT表了，地址分离实现了。

### 2.6 利用IRET指令完成用户栈的切换

-  PC的切换
对于被切换出去的进程，当它再次被调度执行时，根据被切换出去的进程的内核栈的样子，switch_to的最后一句指令`ret`会弹出switch_to()后面的指令`}`作为返回返回地址继续执行，从而执行`}`从schedule()函数返回，将弹出`ret_from_sys_call`作为返回地址执行ret_from_sys_call，在ret_from_sys_call中进行一些处理，最后执行`iret`指令，进行中断返回，将弹出原来用户态进程被中断地方的指令作为返回地址，继续从被中断处执行。
对于得到CPU的新的进程，我们要修改fork.c中的copy_process()函数，将新的进程的内核栈填写成能进行PC切换的样子。根据实验提示，我们可以得到新进程的内核栈的样子，如图所示：

<div  align="center"> <img src="http://img.blog.csdn.net/20150629012631667" width = "300" height = "500" alt="图片名称" align=center /> </div>

注意此处需要和switch_to接在一起考虑，应该从“切换内核栈”完事的那个地方开始，现在到子进程的内核栈开始工作了，接下来做的四次弹栈以及ret处理使用的都是子进程内核栈中的东西。
注意执行ret指令时，这条指令要从内核栈中弹出一个32位数作为EIP跳去执行，所以需要弄出一个个函数地址（仍然是一段汇编程序，所以这个地址是这段汇编程序开始处的标号）并将其初始化到栈中。**既然这里也是一段汇编程序，那么放在system_call.s中是最合适的。**我们弄的一个名为first_return_from_kernel的汇编标号，将这个地址初始化到子进程的内核栈中，现在执行ret以后就会跳转到first_return_from_kernel去执行了。

system_call.s中switch_to的完整代码如下：

```
.align 2
switch_to:
	pushl %ebp
	movl %esp,%ebp
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl 8(%ebp),%ebx
	cmpl %ebx,current
	je 1f
	movl %ebx,%eax
	xchgl %eax,current
	movl tss,%ecx
	addl $4096,%ebx
	movl %ebx,ESP0(%ecx)
	movl %esp,KERNEL_STACK(%eax)
	movl 8(%ebp),%ebx
	movl KERNEL_STACK(%ebx),%esp
	movl 12(%ebp),%ecx	
	lldt %cx
	movl $0x17,%ecx
	mov %cx,%fs
	cmpl %eax,last_task_used_math
	jne 1f
	clts
1:
	popl %eax
	popl %ebx
	popl %ecx
	popl %ebp
	ret
```

system_call.s中first_return_from_kernel代码如下：

```
.align 2
first_return_from_kernel:
	popl %edx
	popl %edi
	popl %esi
	pop %gs
	pop %fs
	pop %es
	pop %ds
	iret
```

fork.c中copy_process()的具体修改如下：

``` c
	......
	p = (struct task_struct *) get_free_page();
	......
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;

	long *krnstack;
	krnstack = (long)(PAGE_SIZE +(long)p);
	*(--krnstack) = ss & 0xffff;
	*(--krnstack) = esp;
	*(--krnstack) = eflags;
	*(--krnstack) = cs & 0xffff;
	*(--krnstack) = eip;
	*(--krnstack) = ds & 0xffff;
	*(--krnstack) = es & 0xffff;
	*(--krnstack) = fs & 0xffff;
	*(--krnstack) = gs & 0xffff;
	*(--krnstack) = esi;
	*(--krnstack) = edi;
	*(--krnstack) = edx;
	*(--krnstack) = (long)first_return_from_kernel;
	*(--krnstack) = ebp;
	*(--krnstack) = ecx;
	*(--krnstack) = ebx;
	*(--krnstack) = 0;
	p->kernelstack = krnstack;
	......
	}
```

最后，注意由于switch_to()和first_return_from_kernel都是在system_call.s中实现的，要想在schedule.c和fork.c中调用它们，就必须在system_call.s中将这两个标号声明为全局的，同时在引用到它们的.c文件中声明它们是一个外部变量。

具体代码如下：

system_call.s中的全局声明
```
.globl switch_to
.globl first_return_from_kernel
```

对应.c文件中的外部变量声明：

```
extern long switch_to;
extern long first_return_from_kernel;
```
