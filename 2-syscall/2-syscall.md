# 系统调用
[TOC]

## close系统调用

```c
_syscall1(int,close,int,fd)

#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a))); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

int close(int fd)
{
long _res;
__asm__volatile ("int $0x80"
	: "=a" (__res)
    : "0" (__NR_close), "b" ((long)(fd)));
if(__res >= 0)
	return (int) __res;
errno = -__res;
return -1;
}
```

## int 0x80内部机制
```c
set_system_gate(0x80,&system_call);

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)
```
展开后：
```c
_set_gate(&idt[0x80], 15, 3, &system_call);
```

```c
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))
```
继续展开，得到最终展开代码

```c
__asm__("movw %%dx, %%ax\n\t"
	"movw %0, %%dx\n\t"
    "movl %%eax, %1\n\t"
    "movl %%edx, %2"
    :
    : "i" ((short) (0x8000+(3<<13)+(15<<8))),
    "o" (*((char *) (&idt[0x80]))),
    "o" (*(4+(char *) (&idt[0x80]))),
    "d" ((char *) (&system_call)), "a" (0x00080000))
//如果要同时执行多条汇编语句，则应该用"\n\t"将各个语句分割开，如上面所示
```

### GCC内联汇编
1. 如果要同时执行多条汇编语句，则应该用"\n\t"将各个语句分隔开，例如：
 ```c
   __asm__( "movw %%dx, %%ax\n\t"
   		"movw %0, %%dx\n\t"
           "movl %%edx, %2")
 ```

2. 通常嵌入到C代码中的汇编语句很难做到与其他部分没有任何关系，因此更多时候需要用到完整的内联汇编格式：
 ```c
   __asm__("asm statements" : outputs : inputs : register-modified)
 ```
 插入到C代码中的汇编语句是以`:`分隔的四个部分，第一部分是汇编代码本身，通常成为指令部。指令部是必须的，而其它部分可以根据实际情况而省略。		
 GCC采用如下方法来解决**汇编代码中操作数如何与C代码中的变量相结合**的问题：对寄存器的使用只需给出**“样板”**和**约束条件**，具体如何将寄存器与变量结合起来完全由GCC和GAS负责。具体而言就是：在指令部，加上前缀`%`的数字(如%0,%1)就是需要使用寄存器的**“样板”**操作数。指令部中使用几个样板操作数，就表明有几个变量需要与寄存器相结合，这样GCC和GAS在编译和汇编时会根据后面给定的**约束条件**进行恰当的处理。由于样板操作数也使用`%`作为前缀，因此寄存器名前面应该加上两个`%`，以免产生混淆。		
 紧跟在指令部后面的是输出部，是规定输出变量如何与样板操作数进行结合的条件，每个条件称为一个“约束”，必要时可以包含多个约束，相互之间用逗号分隔开就可以。每个输出约束都以'='号开始，然后紧跟一个对操作数类型进行说明的字后，最后是如何与变量相结合的约束。凡是与输出部中说明的操作数相结合的寄存器或操作数本身，在执行完嵌入的汇编代码后均不保留执行之前的内容，这是GCC在调度寄存器时所使用的依据。		
 输出部后面是输入部，输入约束与输出约束相似，但不带'='号。如果一个输入约束要求使用寄存器，则GCC在预处理时就会为之分配一个寄存器，并插入必要的指令将操作数装入该寄存器。与输入部中说明的操作数结合的寄存器或操作数本身，在执行完嵌入的汇编代码后也不保留执行之前的内容。		
 在内联汇编中用到的操作数从输出部的第一个约束开始编号，序号从0开始，每个约束计数一次。需要注意的是，内联汇编语句的指令部在引用一个操作数时总是将其作为32位的长字使用，但实际情况可能需要的是字或者字节，因此应该在约束中指明正确的限定符：		

| 限定符                | 意义                       |
| ------------------ | ------------------------ |
| "m", "v", "o"      | 内存单元                     |
| "r"                | 任何寄存器                    |
| "q"                | 寄存器eax, ebx, ecx, edx之一  |
| "i", "h"           | 直接操作数                    |
| "E"和"F"            | 浮点数                      |
| "g"                | 任意                       |
| "a", "b", "c", "d" | 分别表示寄存器eax, ebx, ecx和edx |
| "S"和"D"            | 寄存器esi, edi              |
| "i"                | 常数(0至31)                 |
### 仔细分析`int 0x80`[最终展开代码](#final_code)
```c
__asm__("movw %%dx, %%ax\n\t"
	"movw %0, %%dx\n\t"
    "movl %%eax, %1\n\t"
    "movl %%edx, %2"
    :
    : "i" ((short) (0x8000+(3<<13)+(15<<8))),
    "o" (*((char *) (&idt[0x80]))),
    "o" (*(4+(char *) (&idt[0x80]))),
    "d" ((char *) (&system_call)), "a" (0x00080000))
```

输入部涉及的寄存器有`eax`和`edx`，其初始值如下：

```register
   31                   16 15                     0
   +----------------------+-----------------------+
eax|       0x0008         |        0x0000         |
   +----------------------+-----------------------+

   31                   16 15                     0
   +----------------------+-----------------------+
edx|             &system_call                     |
   +----------------------+-----------------------+   
```

操作数从输出部开始计数，此处输出部为空，所以

```c
%0 = "i" ((short) (0x8000+(3<<13)+(15<<8)))		//立即数
%1 = "o" (*((char *) (&idt[0x80])))			//内存地址
%2 = "o" (*(4+(char *) (&idt[0x80])))		//内存地址
```

其中`%1`和`%2`是`idt`表中`0x80`项对应的8字节描述符中第0个和第4个字节的内存地址。

1. 执行`movw %%dx, %%ax`后，`movw`为字操作运算，eax寄存器的值如下：

```register
 31                   16 15                     0
   +--------------------------+---------------------------+
eax|       0x0008             |&system_call && 0x0000ffff |
   +--------------------------+---------------------------+
```

2. 执行`movw %0, %%dx`后，edx寄存器的值如下：

```register
   31                   16 15                     0
   +---------------------------+----------------------------+
edx|&system_call && 0xffff0000 |    0x8000+(3<<13)+(15<<8)  |
   +---------------------------+----------------------------+ 
```

3. 执行`movl %%eax, %1`和`movl %%edx, %2`后，`movl`为32bit操作指令，`idt`表中`0x80`项对应的描述符的内容如下：

```register
   63                          48 47                            32
   +-----------------------------+--------------------------- ---+
   | &system_call && 0xffff0000  |     0x8000+(3<<13)+(15<<8)    |
   +-----------------------------+-------------------------------+
   31                          16 15                             0
   +-----------------------------+-------------------------------+
   |            0x0008           |  &system_call && 0x0000ffff   |
   +-----------------------------+-------------------------------+   
```

而中断描述符的格式如下：

```register
   63                               48 47 46   44                          32
   +----------------------------------+--+----+--------------------- -------+
   |  interrupt handler entry offset  |P |DPL |                             |
   |             31...16              |  |    |                             |
   +----------------------------------+-------------------------------------+
   31                               16 15                                   0
   +----------------------------------+-------------------------------------+
   |           段选择符                |  interrupt handler entry offset     |
   |                                  |             15...0                  |
   +----------------------------------+-------------------------------------+   
```

### 通过int 0x80中断指令进行系统调用的过程 

所以执行`int 0x80`中断指令的处理流程是：

1. 根据`int 0x80`中断的中断号`0x80`查找`IDT`描述符表的第`0x80`个对应项，得到如上如所示的int 0x80所对应的中断描述符。注意到，该描述符的**DPL=3**，所以用户态程序可以顺利访问该中断描述符表，并跳转往对应的中断处理程序。
2. 注意到跳转到中断处理程序也是段式寻址，**段选择符**为`0x0008`，对应内核代码段所在的描述符表项，而且段描述符`0x0008`对应的`RPL=0`，这样就可以正常访问内核代码段的描述符表项，而内核代码段的基地址为0，所以就等于跳转到`system_call`程序处进行中断处理。

```register
15                          3 2 1     0
+----------------------------+--+--+--+
|        描述符索引           |TI| RPL |        段寄存器/选择子
+----------------------------+--+--+--+
```

3. 在中断处理程序`system_call`中，根据`eax`中保存的系统调用号，调用对应的内核函数(系统调用)。需要时，寄存器`ebx`，`ecx`，`edx`可以保存最多三个参数。
4. 系统调用完成相应功能，返回到中断处理函数`system_call`。
5. 中断处理程序返回到执行`int 0x80`的用户程序中。

## 在用户态和核心态之间传递数据

要实现的两个系统调用参数中都有字符串指针，这些字符串指针都是用户态的数据，在实现系统调用时，我们处于内核态，不能直接访问这些字符串指针。`open(char *filename, ...)`函数中也含有字符串指针参数，所以查看`open()`系统调用的处理。

/lib/open.c

```c
int open(const char * filename, int flag, ...)
{
	register int res;
	va_list arg;

	va_start(arg,flag);
	__asm__("int $0x80"
		:"=a" (res)
		:"0" (__NR_open),"b" (filename),"c" (flag),
		"d" (va_arg(arg,int)));
	if (res>=0)
		return res;
	errno = -res;
	return -1;
}
```

系统调用通过`eax`，`ebx`，`ecx`，`edx`传递参数。其中`eax`传递系统调用号，文件名指针由`ebx`传递。进入内核后，通过`ebx`取出文件名字符串。而open的`ebx`指向的文件名在用户空间，在内核空间不能直接访问该文件名字符串，继续看`open`系统调用的处理。

`int 0x80`会执行`system_call`中断处理程序

/kernel/system_call.s

```assembly
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
```

有上面的代码可以看出，获取**用户地址空间(用户数据段)**中的数据依靠的是**段寄存器fs**。

下面转到`sys_open`执行。

/fs/open.c

```c
int sys_open(const char * filename,int flag,int mode)
{
	......
	if ((i=open_namei(filename,flag,mode,&inode))<0) {
		current->filp[fd]=NULL;
		f->f_count=0;
		return i;
	}
	......
}
```

`sys_open`将文件名参数传给了`open_namei()`。

/fs/namei.c

```c
int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode)
{
	...
	if (!(dir = dir_namei(pathname,&namelen,&basename)))
		return -ENOENT;
	...
}
```

`open_namei`将文件名传给`dir_namei()`。

/fs/namei.c

```c
static struct m_inode * dir_namei(const char * pathname,
	int * namelen, const char ** name)
{
	...
	if (!(dir = get_dir(pathname)))
		return NULL;
	basename = pathname;
	while ((c=get_fs_byte(pathname++)))
		if (c=='/')
			basename=pathname;
	*namelen = pathname-basename-1;
	*name = basename;
	return dir;
}
```

`dir_namei`将文件名又传给了`get_dir()`。

/fs/namei.c

```c
static struct m_inode * get_dir(const char * pathname)
{
	...
	if ((c=get_fs_byte(pathname))=='/') {
		inode = current->root;
		pathname++;
	} 
  	...
}
```

可以看到用`get_fs_byte()`获得一个字节的用户空间中的数据。

查看`get_fs_byte()`的实现。

/include/asm/segment.h

```c
static inline unsigned char get_fs_byte(const char * addr)
{
	unsigned register char _v;

	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}
```

其中：

```assembly
%0 = "=r" (_v)		//输出变量，get_fs_byte()的返回值
%1 = "m" (*addr))	//内存变量，用户地址空间的偏移地址
movb %%fs:%1,%0		//把由fs:addr指向的内存地址的一个字节的数据复制到get_fs_byte()的返回值
```

而使用`put_fs_byte()`可以实现从核心态拷贝一个字节的数据到用户态。

/include/asm/segment.h

```c
static inline void put_fs_byte(char val,char *addr)
{
__asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}
```

`put_fs_byte()`和`get_fs_byte()`正好相反，把内核态一个字节的数据`var`拷贝到由`fs:addr`指向的用户态内存地址空间。

**put_fs_xxx()**：核心态—>用户态

**get_fs_xxx()**：用户态—>核心态

## 用户态测试程序的实现

1. 先参看/lib/close.c中`close`库函数的实现

/lib/close.c

```c
#define __LIBRARY__
#include <unistd.h>

_syscall1(int,close,int,fd)
```

`_syscall1()`是在/include/unistd.h中定义的宏：

```c
#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a))); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}
```

展开之后得到

```c
int close(int fd)
{
long _res;
__asm__volatile ("int $0x80"
	: "=a" (__res)
    : "0" (__NR_close), "b" ((long)(fd)));
if(__res >= 0)
	return (int) __res;
errno = -__res;
return -1;
}
```

可见，用户态`iam()`和`whoami()`的实现只要调用对应的`_syscalln()`即可，`n`为函数对应的参数个数。

2. 其次编译用户态下的两个测试程序`iam.c`和`whoami.c`

```shell
$ gcc -o iam iam.c -Wall
$ gcc -o whoami whoami.c -Wall
```

执行生成的可执行文件，效果如下：

```shell
$ ./iam qiuyu
$ ./whoami
qiuyu
```

可以看出，`iam.c`和`whoami.c`必须都**含有`main()`主函数**，这样执行`./iam qiuyu`才能接受命令行参数。同时`./whoami`才能输出内核保存下来的用户名。

也就是说，`iam.c`中通过`_syscall1(int, iam, const char*, name)`实现`int iam(const char* name)`的函数实现，同时其中的`int main(int argc, char **argv)`实现读取命令行参数，并调用`iam()`把用户名传递给`iam()`函数。

同理，`whoami.c`中通过`_syscall2(int, whoami, char *, name, unsigned int, size)`实现`int whoami(char *name, unsigned int size)`的函数实现，同时其中的`int main()`实现`whoami()`从内核取得的内核保存的用户名，并将其输出显示。

## 内核态iam()和whoami()的实现

内核态`iam()`和`whoami()`就是对应的系统调用`sys_iam()`和`sys_whoami()`的真正实现。

这里注意实验要求：

> int iam(const char* name);
>
> 完成的功能是将字符串参数name的内容拷贝到内核中保存下来。要求name的长度不能超过23个字符。返回值是拷贝的字符数。如果name的字符个数超过了23，则返回“-1”，并置errno为EINVAL。

也就是说，首先要判断`name`的字符个数是否超过23，**如果没有超过23，才将name字符串保存到内核空间。**

同样，`whoami()`的要求

> int whoami(char* name, unsigned int size);
>
> 它将内核中由iam()保存的名字拷贝到name指向的用户地址空间中，同时确保不会对name越界访存（name的大小由size说明）。返回值是拷贝的字符数。如果size小于需要的空间，则返回“-1”，并置errno为EINVAL。

也就是说，首先判断**name的长度是否超过size**，**如果name的值小于size，才将name字符串返回到用户空间，并且打印显示。**

