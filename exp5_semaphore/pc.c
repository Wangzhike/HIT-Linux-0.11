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
