#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>

#define HZ	100

#define PRINT 1

void cpuio_bound(int last, int cpu_time, int io_time);

int main(int argc, char * argv[])
{
	/* 以完全二叉树形式组织子进程 */
	int n1_pid, n2_pid, n3_pid, n4_pid, n5_pid;
	int pid, stat_val;
	int i;
	if ((pid=fork()) < 0) {
		printf("Fork n1 node failed in Root\n");
	} else if (!pid) {	/* n1 */
		if ((pid=fork()) < 0) {
			printf("Fork n3 node failed in n1 node\n");
		} else if (!pid) {	/* n3 */
			#if PRINT
				printf("\t\tN3 node is running...\n");
			#endif
			cpuio_bound(10, 3, 7);
			#if PRINT
				printf("\t\tN3 node is done! Byte\n");
			#endif
			_exit(0);
		} else {	/* n1 */
			#if PRINT
				printf("\tN1 node is running...\n");
			#endif
			n3_pid = pid;
			if ((pid=fork()) < 0) {
				printf("Fork n4 node failed in n1 node\n");
			} else if (!pid) {	/* n4 */
				#if PRINT
					printf("\t\tN4 node is running...\n");
				#endif
				cpuio_bound(10, 4, 6);
				#if PRINT
					printf("\t\tN4 node is done! Bye\n");
				#endif
				_exit(0);
			} else {	/* n1 */
				n4_pid = pid;
				cpuio_bound(10, 1, 0);
				i = 2;
				#if PRINT
					printf("\tN1 node is waiting tow child:\n");
					printf("\t\tN3_node: %d and N4_node: %d\n", n3_pid, n4_pid);
				#endif
				while(0 < i) {
					pid = wait(&stat_val);
					if (pid == n3_pid || pid == n4_pid) {
						i--;
						continue;
					}
				}
				#if PRINT
					printf("\tN1 node is done! Byte\n");
				#endif
				_exit(0);
			}
		}
	} else {	/* root */
		#if PRINT
			printf("Root is running...\n");
		#endif 
		n1_pid = pid;
		if ((pid=fork()) < 0) {
			printf("Fork n2 node failed in Root\n");
		} else if (!pid) {	/* n2 */
			#if PRINT
				printf("\tN2 node is running...\n");
			#endif
			if ((pid=fork()) < 0) {
				printf("Fork n5 node failed in n2 node\n");
			} else if (!pid) {	/* n5 */
				#if PRINT
					printf("\t\tN5 node is running...\n");
				#endif
				cpuio_bound(10, 5, 5);
				#if PRINT
					printf("\t\tN5 node is done! Bye\n");
				#endif
				_exit(0);
			} else {	/* n2 */
				n5_pid = pid;
				cpuio_bound(10, 0, 1);
				#if PRINT
					printf("\tN2 node is waiting one child:\n");
					printf("\t\tN5 node: %d\n", n5_pid);
				#endif
				while(n5_pid != wait(&stat_val));
				#if PRINT
					printf("\tN2 node is done! Byte\n");
				#endif
				_exit(0);
			}
			cpuio_bound(10, 0, 1);
		} else {	/* root */
			n2_pid = pid;
			i = 2;
			#if PRINT
				printf("Root is watiing two child:\n\r");
				printf("\tN1 node: %d and N2 node: %d\r\n", n1_pid, n2_pid);
			#endif
			while(0 < i) {
				pid = wait(&stat_val);
				if(pid == n1_pid || pid == n2_pid) {
					i--;
					continue;
				}
			}
			#if PRINT
				printf("Root is done! Bye\r\n");
			#endif
		}
	}
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

