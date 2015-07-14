# Linux 0.11下信号量的实现和应用	

##1.生产者-消费者问题	
从一个实际的问题：生产者与消费者出发，谈一谈为什么要有信号量？信号量用来做什么？

>	问题描述：现在存在一个文件".\buffer.txt"作为一个共享缓冲区，缓冲区同时最多只能保存10个数。现有一个生产者进程，依次向缓冲区写入整数0，1，2，……，M，M>=500；有N个消费者进程，消费者进程从缓冲区读数，每次读一个，并将读出的数从缓冲区删除。	

*	为什么要有信号量？	
	对于生产者来说，当缓冲区满，也就是空闲缓冲区个数为0时，此时生产者不能继续向缓冲区写数，必须等待，直到有消费者从满缓冲区取走数后，再次有了空闲缓冲区，生产者才能向缓冲区写数。	
	对于消费者来说，当缓冲区空时，此时没有数可以被取走，消费者必须等待，直到有生产者向缓冲区写数后，消费者才能取数。并且如果当缓冲区空时，先后有多个消费者均想从缓冲区取数，那么它们均需要等待，此时需要记录下等待的消费者的个数，以便缓冲区有数可取后，能将所有等待的消费者唤醒，确保请求取数的消费者最终都能取到数。	
	也就是说，当多个进程需要协同合作时，需要根据某个信息，判断当前进程是否需要停下来等待；同时，其他进程需要根据这个信息判断是否有进程在等待，或者有几个进程在等待，以决定是否需要唤醒等待的进程。而这个信息，就是信号量。	
*	信号量用来做什么？	
	设有一整形变量sem，作为一个信号量。此时缓冲区为空，sem=0。	
	1.	消费者C1请求从缓冲区取数，不能取到，睡眠等待。sem=-1<0，表示有一个进程因缺资源而等待。
	2. 消费者C2也请求从缓冲区取数，睡眠等待。sem=-2<0，表示有两个进程因缺资源而等待。
	3. 生产者P往缓冲区写入一个数，sem=sem+1=-1<=0，并唤醒等待队列的头进程C1，C1处于就绪态，C2仍处于睡眠等待。
	4. 生产者P继续往缓冲区写入一个数，sem=0<=0，并唤醒C2，C1、C2就处于就绪状态。	
	由此可见，通过判断sem的值以及改变sem的值，就保证了多进程合作的合理有序的推进，这就是信号量的作用。	
	
##2.	实现信号量	
*	信号量有什么组成？	
	1.	需要有一个整形变量value，用作进程同步。
	2. 需要有一个PCB指针，指向睡眠的进程队列。
	3. 需要有一个名字来表示这个结构的信号量。	

	同时，由于该value的值是所有进程都可以看到和访问的共享变量，所以必须在内核中定义；同样，这个名字的信号量也是可供所有进程访问的，必须在内核中定义；同时，又要操作内核中的数据结构：进程控制块PCB，所以信号量一定要在内核中定义，而且必须是全局变量。由于信号量要定义在内核中，所以和信号量相关的操作函数也必须做成系统调用，还是那句话：**系统调用是应用程序访问内核的唯一方法。**	
*	和信号量相关的函数	
	>Linux在0.11版还没有实现信号量，我们可以先弄一套缩水版的类POSIX信号量，它的函数原型和标准并不完全相同，而且只包含如下系统调用：	
	
	>``` c
	sem_t *sem_open(const char 	*name, unsigned int value);
	int sem_wait(sem_t *sem);
	int sem_post(sem_t *sem);
	int sem_unlink(const char *name);
	```
	sem_t是信号量类型，根据实现的需要自己定义。	
	
*	信号量的保护	
	使用信号量还需要注意一个问题，这个问题是由多进程的调度引起的。当一个进程正在修改信号量的值时，由于时间片耗完，引发调度，该修改信号量的进程被切换出去，而得到CPU使用权的新进程也开始修改此信号量，那么该信号量的值就很有可能发生错误，如果信号量的值出错了，那么进程的同步也会出错。所以在执行修改信号量的代码时，必须加以保护，保证在修改过程中其他进程不能修改同一个信号量的值。也就是说，当一个进程在修改信号量时，由于某种原因引发调度，该进程被切换出去，新的进程如果也想修改该信号量，是不能操作的，必须等待，直到原来修改该信号量的进程完成修改，其他进程才能修改此信号量。修改信号量的代码一次只允许一个进程执行，这样的代码称为临界区，所以信号量的保护，又称临界区保护。
	实现临界区的保护有几种不同的方法，在Linux 0.11上比较简单的方法是通过开、关中断来阻止时钟中断，从而避免因时间片耗完引发的调度，来实现信号量的保护。但是开关中断的方法，只适合单CPU的情况，对于多CPU的情况，不适用。Linux 0.11就是单CPU，可以使用这种方法。	

##3.	信号量的代码实现	
1.	sem_open()	
	原型：`sem_t *sem_open(const char *name, unsigned int value)`	
	功能：创建一个信号量，或打开一个已经存在的信号量	
	参数：
	* name，信号量的名字。不同的进程可以通过同样的name而共享同一个信号量。如果该信号量不存在，就创建新的名为name的信号量；如果存在，就打开已经存在的名为name的信号量。
	* value，信号量的初值，仅当新建信号量时，此参数才有效，其余情况下它被忽略。
	* 返回值。当成功时，返回值是该信号量的唯一标识（比如，在内核的地址、ID等）。如失败，返回值是NULL。	

	由于要做成系统调用，所以会穿插讲解系统调用的相关知识。		
	首先，在linux-0.11/kernel目录下，新建实现信号量函数的源代码文件sem.c。同时，在linux-0.11/include/linux目录下新建sem.h，定义信号量的数据结构。		
	linux-0.11/include/linux/sem.h
	
	``` c
	#ifndef _SEM_H
	#define _SEM_H

	#include <linux/sched.h>

	#define SEMTABLE_LEN	20
	#define SEM_NAME_LEN    20

	typedef struct semaphore{
    	char name[SEM_NAME_LEN];
    	int value;
    	struct task_struct *queue;
	} sem_t;
	extern sem_t semtable[SEMTABLE_LEN];

	#endif
	```
	
	
	由于sem_open()的第一个参数name，传入的是应用程序所在地址空间的逻辑地址，在内核中如果直接访问这个地址，访问到的是内核空间中的数据，不会是用户空间的。所以要用get\_fs\_byte()函数获取用户空间的数据。get\_fs\_byte()函数的功能是获得一个字节的用户空间中的数据。同样，sem\_unlink()函数的参数name也要进行相同的处理。		
2.	sem_unlink()		
	原型：`int sem_unlink(const char *name)`		
	功能：删除名为name的信号量。
	返回值：返回0表示成功，返回-1表示失败		
3.	sem_wait()		
	原型：`int sem_wait(sem_t *sem)`		
	功能：信号量的P原子操作（检查信号量是不是为负值，如果是，则停下来睡眠等待，如果不是，则向下执行）。		
	返回值：返回0表示成功，返回-1表示失败。   
4.  sem_post()		
	原型：`int sem_post(sem_t *sem)`		
	功能：信号量的V原子操作（检查信号量的值是不是为0，如果是，表示有进程在睡眠等待，则唤醒队首进程，如果不是，向下执行）。		
	
	返回值：返回0表示成功，返回-1表示失败。			
### 关于sem\_wait()和sem\_post()

我们可以利用linux 0.11提供的函数sleep\_on()实现进程的睡眠，用wake\_up()实现进程的唤醒。		
但是，sleep\_on()比较难以理解。我们先看下sleep_on()的源码。

``` c
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}
```
还拿生产者和消费者的例子来说，依然是有一个生产者和N个消费者，目前缓冲区为空，没有数可取。

1.	消费者C1请求取数，调用sleep_on(&sem->queue)。此时，tmp指向NULL，p指向C1，调用schedule()，让出CPU的使用权。此时，信号量sem处等待队列的情况如下：		
	<div  align="center"> <img src="http://img.blog.csdn.net/20150714165956872" width = "200" height = "150" alt="图片名称" align=center /> </div>
	
	由于tmp是进程C1调用`sleep_on()`函数时申请的局部变量，所以会保存在C1运行到`sleep_on()`函数中时C1的内核栈中，只要进程C1还没有从`sleep_on()`函数中退出，tmp就会一直保存在C1的内核栈中。**而进程C1是在`sleep_on()`中调用`schedule()`切出去的，所以在C1睡眠期间，tmp自然会保存在C1的内核栈中。这一点对于理解`sleep_on()`上如何形成隐式的等待队列很重要。**
	
2.	消费者C2请求取数，调用sleep_on(&sem->queue)。此时，信号量sem处的等待队列如下：

<div  align="center"> <img src="http://img.blog.csdn.net/20150714170043799" width = "300" height = "150" alt="图片名称" align=center /> </div>
	
	
  >从这里就可以看到隐式的等待队列已经形成了。由于进程C2也会由于调用`schedule()`函数在`sleep_on()`函数中睡眠，所以进程C2内核栈上的tmp便指向之前的等待队列的队首，也就是C1，通过C2的内核栈便可以找到睡眠的进程C1。这样就可以找到在信号量sem处睡眠的所有进程。
	
3.	我们在看下唤醒函数`wake_up()`：

	``` c
	void wake_up(struct task_struct **p)
	{
		if (p && *p) {
			(**p).state=0;
			*p=NULL;
		}
	}
	```

	从中我们可以看到唤醒函数`wake_up()`负责唤醒的是等待队列队首的进程。
	当队首进程C2被唤醒时，从`schedule()`函数退出，执行语句：
	
	``` c
	if (tmp)
		tmp->state=0;
	```
	
	会将内核栈上由tmp指向的进程C1唤醒，如果进程C1的tmp还指向其他睡眠的进程，当C1被调度执行时，会将其tmp指向的进程唤醒，这样只要执行一次`wake_up()`操作，就可以依次将所有等待在信号量sem处的睡眠进程唤醒。		
	
### sem\_wait()和sem\_post()函数的代码实现

由于我们要调用`sleep_on()`实现进程的睡眠，调用`wake_up()`实现进程的唤醒，我们在上面已经讲清楚了`sleep_on()`和`wake_up()`的工作机制，接下来，便可以具体实现`sem_wait()`和`sem_post()`函数了。

1.	sem_wait()的实现		
	考虑到`sleep_on()`会形成一个隐式的等待队列，而`wake_up()`只要唤醒了等待队列的头结点，就可以依靠`sleep_on()`内部的判断语句，实现依次唤醒全部的等待进程。所以，`sem_wait()`的代码实现，必须考虑到这个情况。		
	参考linux 0.11内部的代码，对于进程是否需要等待的判断，不能用简单的if语句，而应该用while()语句，假设现在sem=-1，生产者往缓冲区写入了一个数，sem=0<=0，此时应该将等待队列队首的进程唤醒。当被唤醒的队首进程再次调度执行，从`sleep_on()`函数退出，不会再执行if判断，而直接从if语句退出，继续向下执行。而等待队列后面被唤醒的进程随后也会被调度执行，同样也不会执行if判断，退出if语句，继续向下执行，这显然是不应该的。因为生产者只往缓冲区写入了一个数，被等待队列的队首进程取走了，由于等待队列的队首进程已经取走了那个数，它应该已经将sem修改为sem=-1，其他等待的进程应该再次执行if判断，由于sem=-1<0，会继续睡眠。要让其他等待进程再次执行时，要重新进行判断，所以不能是if语句了，必须是while()语句才可以。		
	下面是我第一次实现`sem_wait()`的代码：
	
	``` c
	int sys_sem_wait(sem_t *sem)
	{
		cli();
		sem->value--;
		while( sem->value < 0 )
			sleep_on(&(sem->queue))
		sti();
		return 0;
	}
	```
	
	但是没有考虑到有一种特殊的信号量：互斥信号量。比如要读写一个文件，一次只能允许一个进程读写，当一个进程要读写该文件时，需要先执行`sem_wait(file)`，此后在该进程读写文件期间，若有其他进程也要读写该文件，则执行流程分析如下：
	
	*	进程P1申请读写该文件，value=-1，`sleep_on(&file->queue)`。
	* 	进程P2申请读写该文件，value=-2，`sleep_on(&file->queue)`。
	* 	原来读写该文件的进程读写完毕，置value=-1，并唤醒等待队列的队首进程P2。
	* 	进程P2再次执行，唤醒进程P1，此时执行while()判断，不能跳出while()判断，继续睡眠等待。此时文件并没有被占用，P2完全可以读写该文件，所以程序运行出错了。出错原因在于，修改信号量的语句，必须放在while()判断的后面，因为执行while()判断，进程有可能睡眠，而这种情况下，是不需要记录有多少个进程在睡眠的，因为`sleep_on()`函数形成的隐式的等待队列已经记录下了进程的等待情况。		
	
	正确的`sem_wait()`代码如下：
	
	``` c
	int sys_sem_wait(sem_t *sem)
	{
		cli();
		while( sem->value <= 0 )		//
			sleep_on(&(sem->queue));	//这两条语句顺序不能颠倒，很重要，是关于互斥信号量能不
		sem->value--;				//能正确工作的！！！
		sti();
		return 0;
	}
	```
	
2.	sem\_post()的实现		
	`sem_post`的实现必须结合`sem_wait()`的实现情况。		
	还拿生产者和消费者的例子来分析。当前缓冲区为空，没有数可取，value=0。		
	*	消费者C1执行`sem_wait()`，value=0，`sleep_on(&queue)`。
	* 	消费者C2执行`sem_wait()`，value=0，`sleep_on(&queue)`。等待队列的情况如下：	
	
  <div  align="center"> <img src="http://img.blog.csdn.net/20150714170158823" width = "250" height = "125" alt="图片名称" align=center /> </div>
		
	* 	生产者执行`sem_post()`，value=1，`wake_up(&queue)`，唤醒消费者C2。队列的情况如下：		
	
  <div  align="center"> <img src="http://img.blog.csdn.net/20150714170424928" width = "300" height = "150" alt="图片名称" align=center /> </div>
	
	*	生产者再次执行`sem_post()`，value=2，`wake_up(&queue)`相当于`wake_up(NULL)`。队列情况如上。
	* 	消费者C2再次执行，唤醒C1，跳出while()，value=1，继续向下执行。
	*  消费者C1再次执行，跳出while()，value=0，继续向下执行。

	由此可以看出，`sem_post()`里面唤醒进程的判断条件是：value<=1。
	
	`sem_post`的实现代码如下：
	
	``` c
	int sys_sem_post(sem_t *sem)
	{
		cli();
		sem->value++;
		if( (sem->value) <= 1)
			wake_up(&(sem->queue));
		sti();
		return 0;
	}
	```
	
### 信号量的完整代码

linux-0.11/kernel/sem.c

``` c
#include <linux/sem.h>
#include <linux/sched.h>
#include <unistd.h>
#include <asm/segment.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
//#include <string.h>

sem_t semtable[SEMTABLE_LEN];
int cnt = 0;

sem_t *sys_sem_open(const char *name,unsigned int value)
{
    char kernelname[100];	/* 应该足够大了 */
    int isExist = 0;
    int i=0;
    int name_cnt=0;
    while( get_fs_byte(name+name_cnt) != '\0')
	name_cnt++;
    if(name_cnt>SEM_NAME_LEN)
	return NULL;
    for(i=0;i<name_cnt;i++)
	kernelname[i]=get_fs_byte(name+i);
    int name_len = strlen(kernelname);
    int sem_name_len =0;
    sem_t *p=NULL;
    for(i=0;i<cnt;i++)
    {
        sem_name_len = strlen(semtable[i].name);
        if(sem_name_len == name_len)
        {
	    		if( !strcmp(kernelname,semtable[i].name) )
	    		{
					isExist = 1;
					break;
	    		}
        }
    }
    if(isExist == 1)
    {
        p=(sem_t*)(&semtable[i]);
        //printk("find previous name!\n");
    }
    else
    {
        i=0;
        for(i=0;i<name_len;i++)
        {
            semtable[cnt].name[i]=kernelname[i];
        }
        semtable[cnt].value = value;
        p=(sem_t*)(&semtable[cnt]);
	     //printk("creat name!\n");
        cnt++;
     }
    return p;
}


int sys_sem_wait(sem_t *sem)
{
	cli();
	while( sem->value <= 0 )		//
		sleep_on(&(sem->queue));	//这两条语句顺序不能颠倒，很重要，是关于互斥信号量能不
	sem->value--;				//能正确工作的！！！
	sti();
	return 0;
}
int sys_sem_post(sem_t *sem)
{
	cli();
	sem->value++;
	if( (sem->value) <= 1)
		wake_up(&(sem->queue));
	sti();
	return 0;
}

int sys_sem_unlink(const char *name)
{
    char kernelname[100];	/* 应该足够大了 */
    int isExist = 0;
    int i=0;
    int name_cnt=0;
    while( get_fs_byte(name+name_cnt) != '\0')
			name_cnt++;
    if(name_cnt>SEM_NAME_LEN)
			return NULL;
    for(i=0;i<name_cnt;i++)
			kernelname[i]=get_fs_byte(name+i);
    int name_len = strlen(name);
    int sem_name_len =0;
    for(i=0;i<cnt;i++)
    {
        sem_name_len = strlen(semtable[i].name);
        if(sem_name_len == name_len)
        {
	    		if( !strcmp(kernelname,semtable[i].name))
	    		{
						isExist = 1;
						break;
	    		}
        }
    }
    if(isExist == 1)
    {
        int tmp=0;
        for(tmp=i;tmp<=cnt;tmp++)
        {
            semtable[tmp]=semtable[tmp+1];
        }
        cnt = cnt-1;
        return 0;
    }
    else
        return -1;
}
```

##4.	实现信号量的系统调用

1.	应用程序包含的宏定义和头文件		
	由于系统调用是借助内嵌汇编\_syscall实现的，而_syscall的内嵌汇编实现是在linux-0.11/include/unistd.h中，所以必须包含`#include <unistd.h>`这个头文件，另外由于_syscall的内嵌汇编实现是包含在一个条件编译里面，所以必须包含这样一个宏定义`#define __LIBRARY__`。
	
2.	修改unistd.h
	添加我们新增的系统调用的编号。		
	添加的代码如下：
	
	``` c
	#define __NR_sem_open	72	/* !!! */
	#define __NR_sem_wait	73
	#define __NR_sem_post	74
	#define __NR_sem_unlink	75
	```
	
3.	修改system_call.s
	由于新增了4个系统调用，所以需要修改总的系统调用的和值。
	修改代码如下：
	
	``` s
	nr_system_calls = 76	/* !!! */
	```
	
4. 修改sys.h		
	要在linux-0.11/include/linux/sys.h中，声明这4个新增的函数。
	修改代码如下：
	
	``` c
	extern int sys_sem_open();
	extern int sys_sem_wait();
	extern int sys_sem_post();
	extern int sys_sem_unlink();

	fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
	sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
	sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
	sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
	sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, 	sys_alarm,
	sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
	sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
	sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, 	sys_setgid,
	sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, 	sys_phys,
	sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
	sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
	sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
sys_setreuid,sys_setregid,sys_sem_open,sys_sem_wait,sys_sem_post,sys_sem_unlink };
	```
	
5.	修改linux-0.11/kernel目录下的Makefile		
	修改代码如下：
	
	``` makefile
	......
	OBJS  = sched.o system_call.o traps.o asm.o fork.o \
	panic.o printk.o vsprintf.o sys.o exit.o \
	signal.o mktime.o sem.o
	......
	### Dependencies:
	sem.s sem.o: sem.c ../include/linux/sem.h ../include/linux/kernel.h \
  	../include/unistd.h
  	......
  	
  	```

6.	在0.11环境下的/usr/include目录下，将修改过的unistd.h文件拷贝覆盖原有的unistd.h文件

##5.	测试用的应用程序的实现
1.	基本要求

	>1.	建立一个生产者进程，N个消费者进程（ N>1 ）
	>2.	用文件建立一个共享缓冲区
	>3.	生产者进程依次向缓冲区写入整数0，1，2，...，M，M>=500
	>4.	消费者进程从缓冲区读数，每次读一个，并将读出的数字从缓冲区删除，然后将本进程ID和+ 数字输出到标准输出
	>5.	缓冲区同时最多只能保存10个数		
	一种可能的输出效果是：		
	10: 0		

2.	文件IO函数

	由于要用文件建立一个共享缓冲区，同时生产者要往文件中写数，消费者要从文件中读数，所以要用到open()、read()、write()、lseek()、close()这些文件IO系统调用。		
	应用程序实现的难点在于，消费者进程每次读一个数，要将读出的数字从缓冲区删除，这几个文件IO系统调用函数中，并没有可以删除一个数字的函数。解决办法是，当消费者进程要从缓冲区读数时，首先调用lseek()系统调用获取到目前文件指针的位置，保存生产者目前写文件的位置。由于被消费者进程读过的数都被删除了，所以同时最多只能保存10个数的缓冲区已有的数，一定是消费者进程未读的，也就是说每次消费者要从缓冲区读数时，要读的数一定是缓冲区的第一个数。这样，让消费者进程每次都从缓冲区读10个数出来，取读出的10个数中的第一个数送标准输出显示，再将后面的9个数再次写入到缓冲区中，这样，就可以做到删除读出的那个数。最后，再调用lseek()系统调用将文件指针定位到之前保存的文件指针减1的位置，这样，生产者进程再次写缓冲区时，也能正确定位删除了一个数字的缓冲区的写位置。
	
3.	终端也是临界资源

	>用printf()向终端输出信息是很自然的事情，但当多个进程同时输出时，终端也成为了临界资源，需要做好互斥保护，否则输出的信息可能错乱。		
	另外，printf()之后，信息只是保存在输出缓冲区内，还没有真正送到终端上，这也可能造成输出信息时序不一致。用fflush(stdout)可以确保数据送到终端。


	
4.	应用程序的实现代码如下：

	``` c
	#define __LIBRARY__
	#include <unistd.h>

	#include <stdio.h>
	#include <linux/sem.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>


	_syscall2(int,sem_open,const char*,name,unsigned int,vaule)
	_syscall1(int,sem_wait,sem_t *,sem)
	_syscall1(int,sem_post,sem_t *,sem)
	_syscall1(int,sem_unlink,const char *,name)

	#define BUFFER_LEN 10
	#define M 60
	#define CONSUMER1	20
	#define CONSUMER2	20
	#define CONSUMER3	20

	/*
 	* producer---father process
 	* consumer1---son1 process
 	* consumer2---son2 processs
 	* consumer3---son3 process
 	*/

	int main(void)
	{
		pid_t father,producer,consumer1,consumer2,consumer3,tmp1,tmp2,tmp3;
		sem_t *p_empty_buf;
		sem_t *p_full_buf;
		sem_t *p_mutex;
		int fd;
		int data[10];
		int pos=0;
		int num=0;
		int a,b,c;
		int i;
		p_empty_buf=(sem_t *)sem_open("Empty",BUFFER_LEN);
		p_full_buf=(sem_t *)sem_open("Full",0);
		p_mutex=(sem_t *)sem_open("Mutex",1);

		if( (fd=open("./buffer.txt",O_RDWR|O_CREAT|O_TRUNC,0644)) <0 ){
			printf("open error\n");
			exit(-1);
		}
		else{
			printf("open success\n");
		}
		tmp1=fork();
		if(tmp1==0)			/* son1---consumer1 */
		{
			consumer1=getpid();
			printf("consumer1 is runing!\n");
			for(a=0;a<CONSUMER1;a++)
			{
				sem_wait(p_full_buf);
				sem_wait(p_mutex);
				if( (pos=lseek(fd,0,SEEK_CUR)) == -1){
					printf("seek pos error\n");
				}
				if( (num = lseek(fd,0,SEEK_SET)) == -1){
					printf("lseek error\n");
				}
				if( (num=read(fd,data,sizeof(int)*10)) == -1){
					printf("read error\n");
				}
				else{
					printf("%d: %d\n",consumer1,data[0]);
				}
				fflush(stdout);
				if( (num = lseek(fd,0,SEEK_SET))== -1){
					printf("lseek error\n");
				}
				if( (num = write(fd,&data[1],sizeof(int)*9)) == -1){
					printf("read and write error\n");
				}
				if( (num = lseek(fd,pos-sizeof(int),SEEK_SET))== -1){
					printf("lseek error\n");
				}
				sem_post(p_mutex);
				sem_post(p_empty_buf);
			}
			printf("Son1 is finished!\n");
		
		}
		else if(tmp1>0)
		{
			tmp2 = fork();
			if(tmp2==0)
			{
				consumer2=getpid();
				printf("consumer2 is runing!\n");
				for(b=0;b<CONSUMER2;b++)
				{
					sem_wait(p_full_buf);
					sem_wait(p_mutex);
					if( (pos=lseek(fd,0,SEEK_CUR)) == -1){
						printf("seek pos error\n");
					}
					if( (num = lseek(fd,0,SEEK_SET)) == -1){
						printf("lseek error\n");
					}
					if( (num=read(fd,data,sizeof(int)*10)) == -1){
						printf("read error\n");
					}
					else{
						printf("%d: %d\n",consumer2,data[0]);
					}
					fflush(stdout);
					if( (num = lseek(fd,0,SEEK_SET))== -1){
						printf("lseek error\n");
					}
					if( (num = write(fd,&data[1],sizeof(int)*9)) == -1){
						printf("read and write error\n");
					}
					if( (num = lseek(fd,pos-sizeof(int),SEEK_SET))== -1){
						printf("lseek error\n");
					}
					sem_post(p_mutex);
					sem_post(p_empty_buf);
				}
				printf("Son2 is finished!\n");
			}
			else if(tmp2>0)
			{
				tmp3 =fork();
				if(tmp3 == 0)
				{
					consumer3 = getpid();
					printf("consumer3 is runing!\n");
					for(c=0;c<CONSUMER3;c++)
					{
						sem_wait(p_full_buf);
						sem_wait(p_mutex);
						if( (pos=lseek(fd,0,SEEK_CUR)) == -1){
							printf("seek pos error\n");
						}
						if( (num = lseek(fd,0,SEEK_SET)) == -1){
							printf("lseek error\n");
						}
						if( (num=read(fd,data,sizeof(int)*10)) == -1){
							printf("read error\n");
						}
						else{
							printf("%d: %d\n",consumer3,data[0]);
						}
						fflush(stdout);
						if( (num = lseek(fd,0,SEEK_SET))== -1){
							printf("lseek error\n");
						}
						if( (num = write(fd,&data[1],sizeof(int)*9)) == -1){
							printf("read and write error\n");
						}
						if( (num = lseek(fd,pos-sizeof(int),SEEK_SET))== -1){
							printf("lseek error\n");
						}
						sem_post(p_mutex);
						sem_post(p_empty_buf);
					}
					printf("Son3 is finished!\n");
				}
				else if(tmp3>0)
				{
					producer=getpid();
					printf("producer is runing!\n");	
					for(i=0;i<M;i++)
					{
						sem_wait(p_empty_buf);	/* P(empty) */
						sem_wait(p_mutex);	/* P(mutex) */
						if( (num=write(fd,&i,sizeof(int))) == -1){
							printf("write error\n");
						}
						/*else{
							printf("write in buffer %d\n",i);
						}*/
						sem_post(p_mutex);	/* V(mutex) */
						sem_post(p_full_buf);	/* V(full) */
					}
					wait((int *)NULL);
					wait((int *)NULL);
					wait((int *)NULL);
					close(fd);
					sem_unlink("Empty");
					sem_unlink("Full");
					sem_unlink("Mutex");
					printf("The father is finished\n");
				}	
				else
					printf("Creat son3 failed\n");
			}
			else
				printf("Creat son2 failed\n");
		}	
		else
			printf("Creat son1 failed\n");
		return 0;
	}
