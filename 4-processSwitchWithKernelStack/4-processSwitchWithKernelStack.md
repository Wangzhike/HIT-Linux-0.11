# 基于内核栈切换的进程切换

1. [实验内容](#实验内容)
2. [实验过程](#实验过程)
    - [实验结果](#实验结果)
	- [实验分析](#实验分析)
	    1. [为什么要从基于TSS的任务切换改为基于内核栈切换的任务切换](#1-为什么要从基于tss的任务切换改为基于内核栈切换的任务切换)
		2. [是否还需要任务状态段TSS](#2-是否还需要任务状态段tss)
		3. [中断和异常的硬件处理](#3-中断和异常的硬件处理)
		4. [进程切换的五段论](#4-进程切换的五段论)
		    1. [利用中断进入内核引起用户栈到内核栈的切换](#41-利用中断进入内核引起用户栈到内核栈的切换)
			2. [引发调度时通过内核栈找到PCB](#42-引发调度时通过内核栈找到pcb)
			3. [找到下一个进程的PCB完成PCB的切换](#43-找到下一个进程的pcb完成pcb的切换)
			4. [通过PCB找到内核栈完成内核栈的切换](#44-通过pcb找到内核栈完成内核栈的切换)
			5. [通过内核栈找到用户栈利用iret中断返回到用户态程序和用户栈](#45-通过内核栈找到用户栈利用iret中断返回到用户态程序和用户栈)
		5. [构造出新建进程能进行切换的内核栈](#5-构造出新建进程能进行切换的内核栈)
		6. [函数调用堆栈](#6-函数调用堆栈)

## 实验内容
正如实验手册所写：    
> 本次实践项目就是将Linux 0.11中采用的TSS切换部分去掉，取而代之的是基于堆栈的切换程序。具体的说，就是将Linux 0.11中的switch_to实现去掉，写成一段基于堆栈切换的代码。    
  本次实验内容如下：    
  - 编写汇编程序`switch_to`
  - 完成主体框架
  - 在主体框架下依次完成PCB切换，内核栈切换，LDT切换等
  - 修改`fork()`，由于是基于内核栈的切换，所以进程需要创建出能完成内核栈切换的样子
  - 修改PCB，即`task_struct`结构，增加相应的内容域，同时处理由于修改了`task_struct`所造成的影响
  - 用修改后的Linux 0.11仍然可以启动，可以正常运行
  - (选做)分析实验3的日志体会修改前后系统运行的差别

  实验报告：    
  回答下面三个问题：    
  1. 针对下面的代码片段：    
  ```c
  movl tss,%ecx
  addl $4096,%ebx
  movl %ebx,ESP0(%ecx)
  ```

  回答问题：(1) 为什么要加4096；(2) 为什么没有设置tss中的`ss0`    
  2. 针对代码片段：    
  ```c
  *(--krnstack) = ebp;
  *(--krnstack) = ecx;
  *(--krnstack) = ebx;
  *(--krnstack) = 0;
  ```

  回答问题：(1) 子进程第一次执行时，`eax=?`为什么要等于这个数？哪里的工作让`eax`等于这样一个数？(2) 这段代码中的`ebx`和`ecx`来自哪里，是什么含义，为什么要通过这些代码将其写到子进程的内核栈中？(3) 这段代码中的`ebp`来自哪里，是什么含义，为什么要做这样的设置？可以不设置吗？为什么？    
3. 为什么要在切换完LDT之后要重新设置`fs=0x17`？而且为什么重设操作要出现在切换完LDT之后，出现在LDT之前又会怎么样？    

## 实验过程

### 实验结果
修改为基于内核栈切换的进程切换后系统运行正常：    
![gcc可以编译生成可执行文件](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/run_1.png)
![process程序正常运行](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/run_2.png)

### 实验分析

#### 1. 为什么要从基于TSS的任务切换改为基于内核栈切换的任务切换
Linux 0.11利用80x86硬件提供的机制：通过执行`ljmp next进程TSS描述符的选择符, (无用的)偏移地址`指令来进行任务切换。这种切换机制的特点正如实验手册所说：    
> 现在的Linux 0.11采用TSS和一条指令就能完成任务切换，虽然简单，但这条指令的执行时间却很长，在实现任务切换时需要200多个时钟周期(一个任务的时间片只有15个时钟周期)。而通过堆栈实现任务切换可能要更快，而且采用堆栈的切换还可以使用指令流水的并行优化技术，同时又使得CPU的设计变得简单。所以无论是Linux还是Windows，进程/线程的切换都没有使用Intel提供的这种TSS切换手段，而都是通过堆栈实现的。   

存在切换时间长，依赖CPU指令支持，单一指令切换无法使能指令流水的并行优化这些问题。而从课程的讲解我们知道：对于函数调用，依靠栈进行返回地址保存和弹栈返回操作；对于用户级线程，每个线程拥有一个线程控制块TCB，TCB关联着用户栈，TCB切换引起用户栈跟着切换，实现从一个线程切换到另一个线程以及再次切换回这个被换出的线程；对于核心级线程，线程切换发生在内核，从用户态进入内核态首先要发生线程用户栈到内核栈的切换，线程的TCB关联着内核栈，TCB切换引起内核栈切换，利用`iret`指令进行中断返回会引起内核栈中用户态参数的出栈，从而实现执行流程转移到新进程的用户态指令和用户栈。这些例子充分说明了栈在指令流程切换中的关键作用，再参照内核`main`函数完成初始化工作后，[以模拟特权级发生变化的内核中断返回的方式手动切换到任务0执行](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/3-processTrack/3-processTrack.md#13-以模拟从特权级发生变化的内核中断处理过程返回的方式手动切换到任务0去执行)，完全可以自己想出是可以利用内核栈切换的方式实现进程切换的。    

#### 2. 是否还需要任务状态段TSS
虽然不再使用`ljmp TSS段选择符的选择子, (不使用的)段内偏移`进行任务切换，但Intel的中断处理机制仍需要保持，因为CPU正是依靠这种机制才能在中断处理时找到内核栈，并将用户态下的`SS:ESP, EFLAGS, CS:EIP`这5个寄存器的值自动压入到内核栈中，这是沟通用户栈(用户态)和内核栈(内核态)的关键桥梁。具体处理过程参见[3. 中断和异常的硬件处理](#3-中断和异常的硬件处理)。所以仍然需要有一个当前TSS，这个TSS就是需要我们额外定义的全局变量tss，即0号进程的tss，所有进程都共用这个tss，任务切换时不再发生变化。    

#### 3. 中断和异常的硬件处理
在实验2：系统调用的[从Linux 0.11自带的库函数入手追寻系统调用的实现过程](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/2-syscall/2-syscall.md#1-从linux-011自带的库函数入手追寻系统调用的实现过程)中介绍的中断或异常的处理过程我们已经知道，当特权级发生变化时，也就是说，当前特权级CPL(存放在CS寄存器的低两位)不同于所选择的段描述符的DPL(从GDT表中获取的段描述符中的描述符特权级DPL)，控制单元必须开始使用与新的特权级相关的栈，要发生栈的切换。通过执行以下步骤来做到这点：    
1. 读`TR`寄存器，里面保存了`TSS`段的选择子，利用该选择子在GDT表中找到运行进程的TSS段的内存位置。    
2. 在`TSS`段中找到新特权级相关的栈段和栈指针即`ss0`和`esp0`，将它们装载到`ss`和`esp`寄存器。    
3. 在新的栈中保存`ss`和`esp`以前的值，这些值定义了旧特权级相关的栈的逻辑地址。形象点说，就是在新栈和旧栈之间拉了一条线，形成了一套栈。    
4. 如果故障已发生，用引起异常的指令地址装载`cs`和`eip`寄存器，从而使得这条指令能再次执行。    
5. 在栈中保存`eflags`,`cs`,`eip`的内容。   
6. 装载`cs`和`eip`寄存器，其值分别是IDT表中由中断号指定的门描述符的段选择符和偏移量字段。这些值给出了中断或异常处理程序的第一条指令的逻辑地址。    

而在中断或异常处理完毕后，相应的处理程序必须产生一条`iret`指令，把控制权转交给被中断的进程，这将迫使控制单元：    
1. 用保存在栈中的值装载`cs`,`eip`和`eflags`寄存器。如果一个硬件出错码曾被压入栈中，并且在`eip`内容的下面，那么，在执行`iret`指令前必须先弹出这个硬件出错码。    
2. 检查处理程序的CPL是否等于`cs`中最低两位的值(这意味着被中断的进程与处理程序运行在同一特权级)。如果是，`iret`终止执行；否则，转入下一步。    
3. 从栈中装载`ss`和`esp`寄存器，因此，返回到与旧特权级相关的栈。    
4. 检查`ds`,`es`,`fs`及`gs`段寄存器的内容，如果其中一个寄存器包含的选择符是一个段选择符，并且其DPL的值小于`CPL`，那么，清相应的段寄存器。控制单元这么做是为了禁止用户态的程序(CPL=3)利用内核以前所用的段寄存器(DPL=0)。如果不清这些寄存器，怀有恶意的用户态程序就可能利用它们来访问内核地址空间。    

#### 4. 进程切换的五段论

##### 4.1 利用中断进入内核引起用户栈到内核栈的切换
前面[3. 中断和异常的硬件处理](#3-中断和异常的硬件处理)已经详细说明了：**利用中断进入内核时CPU通过TR寄存器找到TSS的内存位置，利用里面的`ss0`和`esp0`的值设置好内核栈(此时内核栈是空的，esp0应该设置为内核栈的栈顶地址)，将用户栈的`ss`和`esp`的值压入到内核栈，建立起了用户栈和内核栈的联系，形象点说，即在用户栈和内核栈之间拉了一条线，形成了一套栈。同时将用户态的`eflags`,`cs`,`eip`的值也压入到内核栈，保存用户态程序的返回地址。将由中断号指定的IDT表中门描述符的段选择符和偏移量字段装载到`cs`和`eip`寄存器，所以将跳转到中断或异常处理程序的第一条指令执行，对系统调用而言就是`system_call`中断处理程序**。    
`system_call`接着将`ds`,`es`,`fs`这3个数据段寄存器，以及保存了系统调用参数的`edx`,`ecx`,`ebx`压栈，开始执行相应的系统调用。由于改写后的`fork`系统调用，要创建出新建进程能够完成切换的内核栈的样子，所以我们这里就以`fork`系统调用入手看一下后续的父进程的内核栈变化。    
根据`eax`中保存的系统调用号`__NR_fork`的值，在`sys_call_table`数组中找到`fork`的内核实现函数`sys_fork`，并从`system_call`跳转到`sys_fork`执行，这个过程会将`system_call`的下一条指令的地址压栈。    

`sys_fork`先调用`find_empty_process`为新建进程找到pid，它的值保存在变量`last_pid`中。最终返回新建进程在`task`数组中的下标`nr`。    
返回到`sys_fork`后，它又将寄存器`gs`,`esi`,`edi`,`ebp`的值压栈，再将`find_empty_process`的返回值`nr`压栈，并跳转到`copy_process`执行，这个过程中又将`sys_fork`的下一条指令的地址压栈。所以刚进入到`copy_process`后内核栈的样子如下图所示：    
![刚进入copy_process时内核栈的样子](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-kernelStack_in_copy_process.png)    
`copy_process`是`fork`系统调用的核心实现函数，对`fork`的修改就是在其中加入子进程的内核栈的初始化。    

##### 4.2 引发调度时通过内核栈找到PCB
Linux 0.11把两个不同的数据结构紧凑地存放在一个单独为进程分配的一页内存中：一个是进程描述符PCB，另一个是进程的内核态堆栈。C语言使用下列的联合结构方便地表示一个进程的PCB和内核栈：    
```c
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};
```

其中PCB位于这页内存的低地址，栈位于这页内存的高地址从末端向下增长。另外，当前进程的PCB由全局变量`current`指向。    
当`sys_fork`返回到`system_call`之后，它首先将`sys_fork`的返回值即`last_pid`压栈，然后判断当前进程`current`是否需要调度。如果需要调度，则先将`ret_from_sys_call`函数的地址压栈，然后执行`schedule`调度函数。而在`schedule()`函数的末尾的`}`，相当于`ret`指令，会将`ret_from_sys_call`函数的地址作为返回地址出栈，所以`schedule`函数返回到`ret_from_sys_call`函数执行，而该函数是一段包含了`iret`指令的代码。所以在`system_call`跳转到`schedule`函数执行时的内核栈样子如下：    
![由system_call进入schedule函数内核栈的样子](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-kernelStack_in_schedule.png)

##### 4.3 找到下一个进程的PCB完成PCB的切换
`schedule()`函数通过下面的代码找到下一个进程的PCB：    
```c
while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		if (c) break;	/*　找到一个counter不等于0且是TASK_RUNNING状态中的counter最大的进程；或者当前系统没有一个可以运行的进程，此时c=-1, next=0，进程0得到调度，所以调度算法是不在意进程0的状态是不是TASK_RUNNING，这就意味这进程0可以直接从睡眠切换到运行！ */
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
```

实际上，`next`就是下一个进程在`task`数组中的下标，利用`next`我们就可以找到下一个进程。我按照这个想法进行了实现实验，发现也可以顺利启动系统，但存在写些小问题：    
1. gcc无法编译生成可执行文件    
  用红色方框圈出    
2. process程序运行错误，Root父进程没有等待到子进程N2结束就先结束，N2成为“孤儿进程”    
  用绿色方框圈出    

![next作为参数运行出现问题](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/process-error.png)

而实验手册要求新定义一个PCB结构体的指针变量`pnext`来指向下一个进程，其初始值为进程0的PCB地址`&(init_task.task)`，取这个值是为了保证在系统无事可做时，进程0会得到调度，此时`pnext`就是指向进程0的PCB：    
```c
struct task_struct *pnext = &(init_task.task);
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
				c = (*p)->counter, next = i, pnext = *p;
		}
		if (c) break;	/*　找到一个counter不等于且是TASK_RUNNING状态中的counter最大的进程；或者当前系统没有一个可以运行的进程，此时c=-1, next=0，进程0得到调度，所以调度算法是不在意进程0的状态是不是TASK_RUNNING，这就意味这进程0可以直接从睡眠切换到运行！ */
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
```

而采用`pnext`作为`switch_to`的参数，系统运行就是正常的！！！这里`next`变量和`pnext`应该指的是同一个进程呀！这个问题的原因还有待发现！！！    
下一个进程的PCB指针就是`pnext`，在`switch_to`中通过交换`pnext`和`current`的值就完成了PCB的切换。    

##### 4.4 通过PCB找到内核栈完成内核栈的切换
在进行内核栈切换之前，我们首先对所有进程公用的TSS中的内核栈指针进行重写，将其中表示栈顶指针的`esp0`的值设置为`pnext`的内核栈为空时的栈顶位置，因为从用户态进入内核时内核栈一定是空的。而这个所有进程公用的TSS是一个需要我们新定义的全局变量，和`current`类似：    
```c
struct tss_struct *tss = &(init_task.task.tss);
```

将其初始化为指向进程0的PCB中的TSS结构体成员。由于内核栈的段寄存器`ss0`的值已经在定义`INIT_TASK`宏变量时设置过，其始终是内核数据段的值`0x10`，所以后续在进行TSS中的内核栈重写时不需要改变。    
进行内核栈的切换也很简单，正如实验手册所述：    
> 将寄存器esp(内核栈使用到当前情况时的栈顶位置)的值保存到当前PCB中，再从下一个PCB中的对应位置上取出保存的内核栈栈顶放入esp寄存器，这样处理完毕以后，再使用内核栈时使用的就是下一个进程的内核栈了。    
由于进程的地址空间是隔离的，所以需要切换局部描述符表LDT。而新进程的LDT就是`switch_to`函数的第二个参数，切换过程正如实验手册所述：    
> 指令`movl 12(%ebp), %ecx`负责取出对应`_LDT(next)`的那个参数，指令`lldt %cx`负责修改LDTR寄存器，一旦修改完毕，下一个进程再执行用户态程序时使用的映射表就是自己的LDT表了，地址空间实现了分离。再切换完LDT后还需要执行额外的两句指令`movl $0x17, %ecx mov %cx, %fs`，这两句代码的含义是重新取一下段寄存器fs的值。这两句话必须要加，也必须要出现在切换完LDT之后，这是因为在实验2：系统调用的[在用户态和核心态之间传递数据](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/2-syscall/2-syscall.md#2-在用户态和核心态之间传递数据)中曾经看到过fs的作用——通过fs访问进程的用户态内存，LDT切换完成就意味着切换了分配给进程的用户态内存地址空间，所以前一个fs指向的是上一个进程的用户态内存，而现在需要执行下一个进程的用户态内存，所以就需要用这两条指令来重取fs。不过，细心的读者可能会发现：fs是一个选择子，即fs是一个指向描述符表项的指针，这个描述符才是指向实际的用户态内存的指针，所以上一个进程和下一个进程的fs实际上都是*0x17*，真正找到不同的用户态内存是因为两个进程查的LDT表不一样，所以这样重置以下fs=0x17有用吗，有什么用？这是因为段寄存器包含两个部分：显式部分和隐式部分。段寄存器的隐式部分保存了显式部分所对应的描述符的基地址和段限长，这样如果不是第一个执行含有段寄存器的指令，那么就不需要进行查表，而是直接使用其隐藏部分中的基地址和段限长，提高了执行指令的效率。所以重新取以下fs的值，是为了刷新fs寄存器的隐式部分的内容，使其为新进程的基地址和段限长。    


##### 4.5 通过内核栈找到用户栈利用iret中断返回到用户态程序和用户栈
我们知道`switch_to`函数的最后一条指令是`ret`，执行该指令将返回到下一个进程(目标进程)的`schedule()`函数的末尾，遇到`}`，根据[4.2 引发调度时通过内核栈找到PCB](#42-引发调度时通过内核栈找到pcb)中给出的，在`system_call`跳转到`schedule`函数执行时的内核栈的样子，该`}`相当于`ret`指令将弹出`ret_from_sys_call`的地址，所以将跳转到`ret_from_sys_call`继续执行，它在进行一些信号处理工作后，将一些参数弹栈，最后执行`iret`指令切换到目标进程的用户态程序去执行，用户栈也跟着切换了过去。    

#### 5. 构造出新建进程能进行切换的内核栈的样子
进程切换是由于`schedule`函数末尾调用了`switch_to(pnext, _LDT(next))`，而要为新建进程创建出能够切换的内核栈的样子是在由`sys_fork`调用的`copy_process`中完成的。根据前面[4.2 引发调度时通过内核栈找到PCB](#42-引发调度时通过内核栈找到pcb)给出的在`system_call`完成相应系统调用后跳转到`schedule`函数执行时的内核栈的样子，而正如刚提到的`schedule`又调用了`switch_to(pnext, _LDT(next))`，所以在`switch_to`函数中执行时的内核栈的样子如左图。而根据[4.1 利用中断进入内核引起用户栈到内核栈的切换](#41-利用中断进入内核引起用户栈到内核栈的切换)给出的刚进入`copy_process`后内核栈的样子，如右图所示。    
![switch_to的内核栈](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-kernelStack_in_switch_to.png)
![copy_process的内核栈](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-kernelStack_in_copy_process.png)

左图给出的`switch_to`内核栈中有由`schedule`建立的自己的函数调用堆栈框架，即在栈中压入`system_call`的堆栈基值`ebp`(也就是内核堆栈段寄存器`ss0`的值)。关于函数调用堆栈的详细内容见下面的[6. 函数调用堆栈](#6-函数调用堆栈)。    
对于父进程来说，从`switch_to`返回到`schedule`，再返回到`ret_from_sys_call`，从而回到用户态，这个前面已经分析过了。而子进程的内核栈构造有两种思路：一种是根据实验手册构造内核栈；另一种是将内核栈构造的父进程一样，利用和父进程一样的返回轨迹返回到用户态执行。不论采用哪一种方式，都必须保证新建进程的除了`eax`寄存器(存放`frok`的“返回值”)外，其他的寄存器都恢复到和父进程一样的值(通过pol对应的寄存器实现)，这样才能保证父子进程的状态一样。    
1. 根据实验手册构造内核栈    
  由于`switch_to`中会将`ebp`,`eax`,`ebx`,`ecx`的值压栈，所以此处需要设置好这4个寄存器的值。那么根据右图给出的父进程内核栈的内容，结合实验手册，容易得出子进程内核栈的样子：    
![子进程内核栈](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-kernelStack_child_1.png)

2. 将内核栈构造的和父进程一样    
  由于父进程的内核栈中已经有`ebp`,`eax`,`ebx`,`ecx`的值，而`switch_to`中又要重新压入，但是为了公用父进程的`ret_from_sys_call`返回函数，子进程的内核栈中要两次设置这4个寄存器的值。这种方式下子进程内核栈的样子：    
![子进程内核栈](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-kernelStack_child_2.png)

  而此时`first_return_from_kernel`的代码如下：    
  ```c
  first_return_from_kernel:
	  popl %ebp
	  popl %edi
	  popl %esi
	  pop %gs
	  pushl $ret_from_sys_call
	  ret
  ```

#### 6. 函数调用堆栈
先介绍几个和堆栈操作有关的指令：    
- `pushl %eax`    
  等价于：    
  ```s
  subl $4, %esp
  movl %eax, (%esp)
  ```

- `popl %eax`    
  等价于：    
  ```s
  movl (%esp), %eax
  addl $4, %esp
  ```

- `call 0x12345`    
  ```s
  pushl %eip(*)
  movl $0x12345, %eip(*)	# (*) 表示只是等效，无法替换为该代码
  ```

- `ret`    
  ```s
  popl %eip(*)
  ```

- `enter`        
  ```s
  pushl %ebp
  movl %esp, %ebp
  ```

- `leave`    
  ```s
  movl %ebp, %esp
  popl %ebp
  ```

其中，`ebp`用于记录当前函数调用堆栈基址。    
以`schedule`调用`switch_to`入手分析函数调用堆栈的具体流程：    
1. 由调用者进行函数参数压栈以及返回地址压栈    
  ![调用者压入函数参数和返回地址](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-calling_stack.png)    
  可以看出，函数参数是逆序压入的。这么做的原因是为了解决实际传递的参数数量和被调函数期望接受的参数数量不同的问题。假如顺序压入参数，第1个参数距离被调函数的栈帧指针(`ebp`，属于被调函数的函数调用堆栈)的偏移量就和压入到堆栈的参数数量有关。编译器可以计算出这个值，但还是存在问题——实际传递的参数数量和函数期望接受的参数数量可能并不相同(回想下`exec`系列系统调用中`execl`,`execlp`,`execle`这3个都是可变参数函数，其都是基于`execve`系统调用的C语言库中封装的函数。`execve`系统调用的参数只有3个，而这3个函数的参数个数不受限制)。在这种情况下，这个偏移量就是不正确的，当函数试图访问一个参数时，它实际所访问的将不是它想要的那个。    

2. 由被调用函数建立自己的函数调用框架    
  ![被调函数的函数调用框架](https://github.com/Wangzhike/HIT-Linux-0.11/raw/master/4-processSwitchWithKernelStack/picture/4-called_stack.png)    
  `switch_to`一上来先执行`pushl %ebp movl %esp, %ebp`建立自己的函数调用框架。    
3. 由被调用函数在返回前拆除自己的函数调用框架    
  `switch_to`在执行`ret`指令前通过`popl %eax popl %ebx popl %ecx popl %ebp`拆除自己的函数调用框架。    
4. 由调用者进行函数参数的弹出操作    
  `schedule`是C函数，会自动处理函数调用框架。最后的`}`，会拆除函数调用框架。    
  
