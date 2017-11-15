#include <asm/segment.h>
#include <errno.h>

#define KNAME_LEN	23
char kernel_name[KNAME_LEN];

int sys_iam(const char *name)
{
	int i = 0;
	int res = 0;
	int len = 0;
	// printk("This the kernel, now We are in sys_iam!\n");
	while( get_fs_byte(name+len) != '\0')
		len++;
	if(len <= KNAME_LEN){
		while( (kernel_name[i] = get_fs_byte(name+i)) != '\0' )
			i++;
		res = i;
		// printk("Ok, we have gotten name string:%s from user space.\n", kernel_name);
		// printk("The len of gotton name:%s is: %d\n", kernel_name, res);
	}else{
		errno = EINVAL;
		res = -1;
		// printk("Sorry, the name string is too long!\n");
	}
	return res;
}

int sys_whoami(char *name, unsigned int size)
{
	int i = 0;
	int res = 0;
	int len = 0;
	// printk("This the kernel, now We are in sys_whoami!\n");
	while( kernel_name[len++] != '\0');
	if(len <= size){
		for(i=0; kernel_name[i] != '\0'; i++)
			put_fs_byte(kernel_name[i], name+i);
		res = i;
		// printk("Ok, we have send name string:%s to user space.\n", kernel_name);
		// printk("The len of send name:%s is: %d\n", kernel_name, res);
	}else{
		errno = EINVAL;
		res = -1;
		// printk("Sorry, the size is too short!\n");
	}
	return res;
}
