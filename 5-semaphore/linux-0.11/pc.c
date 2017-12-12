#define __LIBRARY__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/sem.h>

#define SIZE 10
#define M 1000

_syscall2(int, sem_open, const char*, name, unsigned int, value)
_syscall1(int, sem_wait, sem_t *, sem)
_syscall1(int, sem_post, sem_t *, sem)
_syscall1(int, sem_unlink, const char *, name)

/*
 * 消费者是按照从0开始依次递增的顺序往缓冲区写的，缓冲区长度为10，
 * 每次写满缓冲区后再从缓冲区开头开始写。多个消费者是跟随调度随机
 * 读取缓冲区数据的，为了协调多个消费者进程从哪开始读取，在文件中
 * 单独留出一个位置存放消费者上一次读取到的值val，由于生产者的写
 * 顺序，当前的消费者根据上一次消费者读到的val值，就可以得到上一次
 * 消费者的文件读取位置，加1之后就是当前的读取位置。并且根据val的
 * 值，可以知道上一次消费者是否已经读取完毕了，即val == 600。如果
 * 是，说明生产者已经写完毕退出了，消费者就不用继续执行P操作等待
 * 这个已经退出的生产者，可以直接退出了。
 */
int main(void)
{
	pid_t pid;
	int cnt = 0;	/* productor count vairable */
	int children = 6;	/* main process's child number */
	int val = -1;	/* the read value for all consumers */
	int pos;		/* the file offset between productor and consumers */
	int res;		/* returned result */
	int bf;			/* file descriptor for mutual buffer */
	sem_t *sem_empty, *sem_full, *sem_mutex, *sem_atom;
	int i;
	/* 打开文件 */
	bf = open("buffer.txt", O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);
	if (bf == -1) {
		perror("Create buffer file failed!");
		exit(EXIT_FAILURE);
	}
	/* 将消费者读取的初始值-1写入到文件 */
	res = lseek(bf, 0, SEEK_SET);
	if (res == -1) {
		perror("Main lseek failed!");
		exit(EXIT_FAILURE);
	}
	if ((res = write(bf, (void *)(&val), sizeof(val))) == -1) {
		perror("Main write failed!");
		exit(EXIT_FAILURE);
	}

	/* 分别创建缓冲区空闲数empty，缓冲区满数full，文件读写互斥量mutex 3个信号量 */
	sem_empty = (sem_t *)sem_open("empty", SIZE);
	if (sem_empty == SEM_FAILED) {
		perror("Create semaphore empty failed!");
		exit(EXIT_FAILURE);
	}
	sem_full = (sem_t *)sem_open("full", 0);
	if (sem_full == SEM_FAILED) {
		perror("Create semaphore full failed!");
		exit(EXIT_FAILURE);
	}
	sem_mutex = (sem_t *)sem_open("mutex", 1);
	if (sem_mutex == SEM_FAILED) {
		perror("Create semaphore mutex failed!");
		exit(EXIT_FAILURE);
	}
	sem_atom = (sem_t *)sem_open("atom", 1);
	if (sem_atom == SEM_FAILED) {
		perror("Create semaphore atom failed!");
		exit(EXIT_FAILURE);
	}

	if ((pid=fork()) == -1) {
		perror("Productor fork failed!");
		exit(EXIT_FAILURE);
	}
	/* Productor */
	else if (!pid) {
		while (cnt <= M) {
			/* P操作 */
			sem_wait(sem_empty);
			sem_wait(sem_mutex);
			pos = (cnt % SIZE) + 1;
			res = lseek(bf, pos*sizeof(pos), SEEK_SET);
			if (res == -1) {
				perror("Productor lseek failed!");
				exit(EXIT_FAILURE);
			}
			if ((res = write(bf, (void *)(&cnt), sizeof(cnt))) == -1) {
				perror("write failed!");
				exit(EXIT_FAILURE);
			}
			/* V操作 */
			sem_post(sem_mutex);
			sem_post(sem_full);
			cnt++;
		}
		exit(EXIT_SUCCESS);
	} else {
		for (i = 0; i < children - 1; i++) {
			if ((pid = fork())== -1) {
				perror("fork consumer n failed");
				exit(EXIT_FAILURE);
			}
			/* Consumer n */
			else if (!pid) {
				do {
					sem_wait(sem_atom);
					/* 读取上次写入的数值 */
					sem_wait(sem_mutex);
					res = lseek(bf, 0, SEEK_SET);
					if (res == -1) {
						perror("Consumer 1 lseek failed!");
						exit(EXIT_FAILURE);
					}
					if ((res = read(bf, (void *)(&val), sizeof(val))) == -1) {
						perror("Consumer 1 read failed!");
						exit(EXIT_FAILURE);
					}
					sem_post(sem_mutex);
					if (val < M) {
						/* P操作 */
						sem_wait(sem_full);
						sem_wait(sem_mutex);
						pos = (val + 1) % SIZE + 1;
						res = lseek(bf, pos*sizeof(int), SEEK_SET);
						if (res == -1) {
							perror("Consumer 1 lseek failed!");
							exit(EXIT_FAILURE);
						}
						if ((res = read(bf, (void *)(&val), sizeof(val))) == -1) {
							perror("Consumer 1 read failed!");
							exit(EXIT_FAILURE);
						} 
						printf("%d:%d\n", getpid(), val);
						fflush(stdout);
						/* V操作 */
						sem_post(sem_mutex);
						sem_post(sem_empty);
						/* 把两个消费者共享的读取值val写入到文件，用于共享 */
						sem_wait(sem_mutex);
						res = lseek(bf, 0, SEEK_SET);
						if (res == -1) {
							perror("Consumer 1 lseek failed!");
							exit(EXIT_FAILURE);
						}
						if ((res = write(bf, (void *)(&val), sizeof(val))) == -1) {
							perror("Consumer 1 write failed!");
							exit(EXIT_FAILURE);
						}
						sem_post(sem_mutex);
					}
					sem_post(sem_atom);
				} while (val < M);
				exit(EXIT_SUCCESS);
			} 
		}
		while (children--)
			wait(NULL);
		close(bf);
		sem_unlink("empty");
		sem_unlink("full");
		sem_unlink("mutex");
		sem_unlink("atom");
		exit(EXIT_SUCCESS);
	}
}
