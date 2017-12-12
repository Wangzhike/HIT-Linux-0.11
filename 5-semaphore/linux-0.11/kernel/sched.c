/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1<<((nr)-1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);
extern void switch_to(struct task_struct *pnext, unsigned long ldt);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task);
struct tss_struct *tss = &(init_task.task.tss);
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct *pnext = &(init_task.task);
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE) 
				{
					(*p)->state=TASK_RUNNING;
					/* print message to show the waiting process is ready again in 3-processTrack */
						/* print waiting process is ready to stdout */
					/* fprintk(1, "The ID of ready process is %ld in %s\n", (*p)->pid, "schedule"); */
						/* print ready process track to process.log */
					/* fprintk(3, "%ld\t%c\t%ld\t%s\n", (*p)->pid, 'J', jiffies, "schedule"); */
					fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies);
					/* print message end in 3-processTack */
				}
		}

/* this is the scheduler proper: */

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
	if (task[next]->pid != current->pid) {
		if (current->state == TASK_RUNNING) {
			/* print message to show the ready process is running again in 3-processTrack */
				/* print running process is ready to stdout */
			/* fprintk(1, "The ID of running process is %ld\n in %s", current->pid, "schedule"); */
			/* print running process track to process.log */
			/* fprintk(3, "%ld\t%c\t%ld\t%s\n", current->pid, 'J', jiffies, "schedule"); */
			fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'J', jiffies);
		}
	
			/* print ready process is running to stdout */
		/* fprintk(1, "The ID of running process is %ld in %s\n", task[next]->pid, "schedule"); */
			/* print running process track to process.log */
		/* fprintk(3, "%ld\t%c\t%ld\t%s\n", task[next]->pid, 'R', jiffies, "schedule"); */
		fprintk(3, "%ld\t%c\t%ld\n", task[next]->pid, 'R', jiffies);

		/* print message end in 3-processTack */
	} 
	/* else {
		if (task[next]->pid == 0) {
			fprintk(3, "%ld\tJ\t%ld\n", current->pid, jiffies);

			fprintk(3, "%ld\tR\t%ld\n", current->pid, jiffies);

		}
	} */
	switch_to(pnext, _LDT(next));
	//switch_to(task[next], _LDT(next));
}

/*
 * 系统无事可做的时候，进程0会不停地调用sys_pause()，以激活调度算法。此时它的状态可以是等待态，
 * 等待有其他可运行的进程；也可以是运行态，因为它是唯一一个在CPU上运行的进程，只不过运行的效果是等待。
 */
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	if (current->pid != 0) 
	{
		/* print message to show the running process sleeps in 3-processTrack */
			/* print running process sleeps to stdout */
		/* fprintk(1, "The ID of sleep process is %ld in %s\n", current->pid, "sys_pause"); */
			/* print sleeping process track to process.log */
		/* fprintk(3, "%ld\t%c\t%ld\t%s\n", current->pid, 'W', jiffies, "sys_pause"); */
		fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
		/* print message end in 3-processTack */
	} 
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;	/* 仔细阅读，实际上是将current插入“等待队列”头部，tmp是原来的头部 */
	current->state = TASK_UNINTERRUPTIBLE;
	/* print message to show the running process sleeps in 3-processTrack */
		/* print running process sleeps to stdout */
	/* fprintk(1, "The ID of sleep process is %ld in %s\n", current->pid, "sleep_on"); */
		/* print sleeping process track to process.log */
	/* fprintk(3, "%ld\t%c\t%ld\t%s\n", current->pid, 'W', jiffies, "sleep_on"); */
	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
	/* print message end in 3-processTack */
	schedule();
	if (tmp) {
		/* print message to show the sleeping process is ready in 3-processTrack */
			/* print sleeping process is ready to stdout */
		/* fprintk(1, "The ID of ready process is %ld in %s\n", tmp->pid, "sleep_on"); */
			/* print ready process track to process.log */
		/* fprintk(3, "%ld\t%c\t%ld\t%s\n", tmp->pid, 'J', jiffies, "sleep_on"); */
		fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
		/* print message end in 3-processTack */
		tmp->state=0;	/* 唤醒队列中的上一个(tmp)睡眠进程。0换作TASK_RUNNING更好。在记录进程被唤醒时一定要考虑到这种情况，实验者一定要注意！！！ */
	}
}

/*
 * TASK_UNINTERRUPTIBLE和TASK_INTERRUPTIBLE的区别在于不可中断的睡眠只能由wake_up()显示唤醒，
 * 再由上面的 schedule()语句后面的
 *
 * if (tmp) tmp->state=0;
 *
 * 依次唤醒，所以不可中断的睡眠一定是严格按照从“队列”(一个依靠放在进程内核栈中的指针变量tmp
 * 维护的队列)的首部进行唤醒。而对于可中断的进程，除了用wake_up唤醒以外，也可以用信号(给进程
 * 发送一个信号，实际上就是将进程PCB中维护的一个向量的某一位置位，进程需要在合适的时候处理
 * 这一位。感兴趣的实验者可以阅读有关代码)来唤醒，如在 schedule()中：
 *
 * for(p = &LAST_TASK; p > &FRIST_TASK; --p)
 * 		if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
 * 			(*p)->state==TASK_INTERRUPTIBLE)
 * 			(*p)->state=TASK_RUNNING;//唤醒
 *
 * 就是当进程是可中断睡眠时，如果遇到一些信号就将其唤醒。这样的唤醒会出现一个问题，那就是可能会
 * 唤醒等待队列中间的某个进程，此时这个链就需要进行适当调整。interruptible_sleep_on和sleep_on
 * 函数的主要区别就在这里。
 */
void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	/* print message to show the running process sleeps in 3-processTrack */
		/* print running process sleeps to stdout */
	/* fprintk(1, "The ID of sleep process is %ld in %s\n", current->pid, "interruptible_sleep_on"); */
		/* print sleeping process track to process.log */
	/* fprintk(3, "%ld\t%c\t%ld\t%s\n", current->pid, 'W', jiffies, "interruptible_sleep_on"); */
	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
	/* print message end in 3-processTack */
	schedule();
	if (*p && *p != current) {	/* 如果队列头进程和刚唤醒的进程current不是一个，说明从队列中间唤醒了一个进程，需要处理 */
		(**p).state=0;	/* 将队列头唤醒，并通过goto repeat让自己再去睡眠 */
		/* print message to show the sleeping process is ready in 3-processTrack */
			/* print sleeping process is ready to stdout */
		/* fprintk(1, "The ID of ready process is %ld in %s\n", (*p)->pid, "interruptible_sleep_on"); */
			/* print ready process track to process.log */
		/* fprintk(3, "%ld\t%c\t%ld\t%s\n", (*p)->pid, 'J', jiffies, "interruptible_sleep_on"); */
		fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies);
		/* print message end in 3-processTack */
		goto repeat;
	}
	*p=NULL;
	if (tmp) {
		tmp->state=0;	/* 作用和sleep_on函数中的一样 */
		/* print message to show the sleeping process is ready in 3-processTrack */
			/* print sleeping process is ready to stdout */
		/* fprintk(1, "The ID of ready process is %ld in %s\n", tmp->pid, "interruptible_sleep_on"); */
			/* print ready process track to process.log */
		/* fprintk(3, "%ld\t%c\t%ld\t%s\n", tmp->pid, 'J', jiffies, "interruptible_sleep_on"); */
		fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies);
		/* print message end in 3-processTack */
	}
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		/* print message to show the sleeping process is ready in 3-processTrack */
			/* print sleeping process is ready to stdout */
		/* fprintk(1, "The ID of ready process is %ld in %s\n", (*p)->pid, "wake_up"); */
			/* print ready process track to process.log */
		/* fprintk(3, "%ld\t%c\t%ld\t%s\n", (*p)->pid, 'J', jiffies, "wake_up"); */
		fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies);
		/* print message end in 3-processTack */
		*p=NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);
	lldt(0);
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);
}
