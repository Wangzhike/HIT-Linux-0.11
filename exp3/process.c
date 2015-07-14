#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/types.h>

#define HZ	100

void cpuio_bound(int last, int cpu_time, int io_time);

int main(int argc, char * argv[])
{
	pid_t father,son1,son2,son3,tmp1,tmp2,tmp3;
	tmp1=fork();
	if(tmp1==0)			/* son1 */
	{
		son1=getpid();
		printf("The son1's pid:%d\n",son1);
		printf("I am son1\n");
		cpuio_bound(10, 3, 2);
		printf("Son1 is finished\n");
	}
	else if(tmp1>0)
	{
		son1=tmp1;
		tmp2=fork();
		if(tmp2==0)		/* son2 */
		{
			son2=getpid();
			printf("The son2's pid:%d\n",son2);
			printf("I am son2\n");
			cpuio_bound(5, 1, 2);
			printf("Son2 is finished\n");
		}
		else if(tmp2>0)		/* father */
		{
			son2=tmp2;
			father=getpid();
			printf("The father get son1's pid:%d\n",tmp1);
			printf("The father get son2's pid:%d\n",tmp2);
			wait((int *)NULL);
			wait((int *)NULL);
			printf("Now is the father's pid:%d\n",father);
		}
		else
			printf("Creat son2 failed\n");
	}
	else
		printf("Creat son1 failed\n");
	return 0;
}

/*
 * 此函数按照参数占用CPU和I/O时间
 * last: 函数实际占用CPU和I/O的总时间，不含在就绪队列中的时间，>=0是必须的
 * cpu_time: 一次连续占用CPU的时间，>=0是必须的
 * io_time: 一次I/O消耗的时间，>=0是必须的
 * 如果last > cpu_time + io_time，则往复多次占用CPU和I/O
 * 所有时间的单位为秒
 */
void cpuio_bound(int last, int cpu_time, int io_time)
{
	struct tms start_time, current_time;
	clock_t utime, stime;
	int sleep_time;

	while (last > 0)
	{
		/* CPU Burst */
		times(&start_time);
		/* 其实只有t.tms_utime才是真正的CPU时间。但我们是在模拟一个
		 * 只在用户状态运行的CPU大户，就像“for(;;);”。所以把t.tms_stime
		 * 加上很合理。*/
		do
		{
			times(&current_time);
			utime = current_time.tms_utime - start_time.tms_utime;
			stime = current_time.tms_stime - start_time.tms_stime;
		} while ( ( (utime + stime) / HZ )  < cpu_time );
		last -= cpu_time;

		if (last <= 0 )
			break;

		/* IO Burst */
		/* 用sleep(1)模拟1秒钟的I/O操作 */
		sleep_time=0;
		while (sleep_time < io_time)
		{
			sleep(1);
			sleep_time++;
		}
		last -= sleep_time;
	}
}

