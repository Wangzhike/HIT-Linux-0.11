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
