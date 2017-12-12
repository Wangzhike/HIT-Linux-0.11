#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#define SIZE 10
#define M 600

void my_exit(void)
{
	sem_unlink("empty");
	sem_unlink("full");
	sem_unlink("mutex");
	sem_unlink("atom");
}

int main(void)
{
	pid_t pid;
	int cnt = 0;
	int children = 6;
	int val = -1;
	int pos;
	int res;
	int bf;
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
	if ((res = write(bf, &val, sizeof(val))) == -1) {
		perror("Main write failed!");
		exit(EXIT_FAILURE);
	}

	/* 分别创建缓冲区空闲数empty，缓冲区满数full，文件读写互斥量mutex 3个信号量 */
	sem_empty = sem_open("empty", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, SIZE);
	if (sem_empty == SEM_FAILED) {
		perror("Create semaphore empty failed!");
		exit(EXIT_FAILURE);
	}
	sem_full = sem_open("full", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
	if (sem_full == SEM_FAILED) {
		perror("Create semaphore full failed!");
		exit(EXIT_FAILURE);
	}
	sem_mutex = sem_open("mutex", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);
	if (sem_mutex == SEM_FAILED) {
		perror("Create semaphore mutex failed!");
		exit(EXIT_FAILURE);
	}
	sem_atom = sem_open("atom", O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);
	if (sem_atom == SEM_FAILED) {
		perror("Create semaphore atom failed!");
		exit(EXIT_FAILURE);
	}

	/* 设置退出函数 */
	if (atexit(my_exit) == -1) {
		perror("atexit failed!");
	}

	if ((pid=fork()) == -1) {
		perror("Productor fork failed!");
		exit(EXIT_FAILURE);
	} else if (!pid) {	/* productor */
		while (cnt <= M) {
			/*
			 * P操作
			 */
			sem_wait(sem_empty);
			sem_wait(sem_mutex);
			pos = (cnt % SIZE) + 1;
			res = lseek(bf, pos*sizeof(pos), SEEK_SET);
			if (res == -1) {
				perror("Productor lseek failed!");
				exit(EXIT_FAILURE);
			}
			if ((res = write(bf, &cnt, sizeof(cnt))) == -1) {
				perror("write failed!");
				exit(EXIT_FAILURE);
			}
			/*
			 * V操作
			 */
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
			else if (!pid) {	// Consumer n
				do {
					sem_wait(sem_atom);
					//读取上次写入的数值
					sem_wait(sem_mutex);
					res = lseek(bf, 0, SEEK_SET);
					if (res == -1) {
						perror("Consumer 1 lseek failed!");
						exit(EXIT_FAILURE);
					}
					if ((res = read(bf, &val, sizeof(val))) == -1) {
						perror("Consumer 1 read failed!");
						exit(EXIT_FAILURE);
					}
					sem_post(sem_mutex);
					if (val < M) {
						//P操作
						sem_wait(sem_full);
						sem_wait(sem_mutex);
						pos = (val + 1) % SIZE + 1;
						res = lseek(bf, pos*sizeof(int), SEEK_SET);
						if (res == -1) {
							perror("Consumer 1 lseek failed!");
							exit(EXIT_FAILURE);
						}
						if ((res = read(bf, &val, sizeof(val))) == -1) {
							perror("Consumer 1 read failed!");
							exit(EXIT_FAILURE);
						} 
						printf("%d:%d\n", getpid(), val);
						fflush(stdout);
						//V操作
						sem_post(sem_mutex);
						sem_post(sem_empty);
						//把两个消费者共享的读取值val写入到文件，用于共享
						sem_wait(sem_mutex);
						res = lseek(bf, 0, SEEK_SET);
						if (res == -1) {
							perror("Consumer 1 lseek failed!");
							exit(EXIT_FAILURE);
						}
						if ((res = write(bf, &val, sizeof(val))) == -1) {
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
		sem_close(sem_atom);
		sem_close(sem_empty);
		sem_close(sem_full);
		sem_close(sem_mutex);
		exit(EXIT_SUCCESS);
	}
}
