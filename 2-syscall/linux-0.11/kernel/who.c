#include <asm/segment.h>
#include <errno.h>

#include <linux/kernel.h>

#define NAMELEN 23

char username[NAMELEN+1];

int sys_iam(const char *name)
{
	unsigned int namelen = 0;
	int i = 0;
	int res = -1;
	//printk("Now we in kernel's sys_iam\n");
	while (get_fs_byte(name+namelen) != '\0')
		namelen++;
	if (namelen <= NAMELEN) {
		//printk("All %d user space's chars to be copied to the kernel\n", namelen);
		//printk("Copying from user to kernel...\n");
		for(i = 0; i < namelen; i++) {
			username[i] = get_fs_byte(name+i);
		}
		//printk("Done!\n");
		username[i] = '\0';
		//printk("%s\n", username);
		res = namelen;
	} else {
		printk("Error, the user space's name's length is %d longer than 23!\n", namelen);
		res = -(EINVAL);
	}
	return res;
}


int sys_whoami(char *name, unsigned int size)
{
	unsigned int namelen = 0;
	int i = 0;
	int res = -1;
	//printk("Now we in kernel's sys_whoami\n");
	while(username[namelen] != '\0')
		namelen++;
	if (namelen < size) {
		//printk("All %d kernel's chars to be copied to user space\n", namelen);
		//printk("Copying from kernel to user...\n");
		for (i = 0; i < namelen; i++) {
			put_fs_byte(username[i], name+i);
		}
		//printk("Done!\n");
		put_fs_byte('\0', name+i);
		res = namelen;
	} else {
		printk("Error, the kernel's name's length is longer than %d\n", size);
		res = -(EINVAL);
	}
	return res;
}
