#include <asm/system.h>
#include <asm/segment.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/sem.h>

sem_t sems[_SEM_MAX];

sem_t *sys_sem_open(const char *name, unsigned int value)
{
	char kname[_SEM_NAME_MAX+1];
	unsigned int namelen = 0;
	unsigned int len = 0;
	int sem_idx = -1;
	int has = 0;
	int i, j;
	while (get_fs_byte(name+namelen) != '\0')
		namelen++;
	if (_SEM_NAME_MAX < namelen) {
		errno = EINVAL;
		return SEM_FAILED;
	}
	/* 获取信号量的名字 */
	for (i = 0; i < namelen; i++)
		kname[i] = get_fs_byte(name+i);
	kname[i] = '\0';
	/* 在sems数组中搜索是否有该名字 */
	for (i = 0, has = 0; i < _SEM_MAX; i++) {
		if (!strcmp(kname, sems[i].name)) {
			has = 1;
			break;
		}
	}
	if (has) {
		sems[i].cnt++;
		return &sems[i];
	}
	/* 以kname为参数创建该信号量 */
	for (i = 0; i < _SEM_MAX; i++) {
		if (sems[i].name[0] == '\0') {
			sem_idx = i;
			break;
		}
	}
	if (sem_idx == -1)
		return SEM_FAILED;
	for (i = 0; i <= namelen; i++) {
		sems[sem_idx].name[i] = kname[i];
	}
	sems[sem_idx].value = value;
	sems[sem_idx].cnt = 1;
	initqueue(&sems[sem_idx].squeue);
	return &sems[sem_idx];
}

int sys_sem_wait(sem_t *sem)
{
	int res = 0;
	/* 屏蔽中断保护信号量 */
	cli();
	sem->value--;
	/* 信号量计数值value < 0，表明缺少资源，进程阻塞 */
	if (sem->value < 0) {
		sti();
		if (sem->squeue.enqueue(&sem->squeue, current) == -1) {
			res = -1;
		} else {
			if (current == FIRST_TASK)
				panic("task[0] trying to sleep");
			current->state = TASK_UNINTERRUPTIBLE;
			schedule();
		}
	} else
		sti();
	return res;
	/*while (sem->value <= 0)
		sleep_on(&(sem->queue));
	sem->value--;
	sti();
	return 0;*/
}

int sys_sem_post(sem_t *sem)
{
	struct task_struct *p;
	int res = 0;
	/* 屏蔽中断信号保护信号量 */
	cli();
	sem->value++;
	/* 信号量计数值value <= 0，表明之前有进程阻塞，需要唤醒队列头进程 */
	if (sem->value <= 0) {
		sti();
		if (sem->squeue.dequeue(&sem->squeue, &p) == -1) {
			res = -1;
		} else {
			p->state = TASK_RUNNING;
		}
	} else
		sti();
	return res;
	/*if (sem->value <= 1)
		wake_up(&(sem->queue));
	sti();
	return 0;*/
}

int sys_sem_unlink(const char *name)
{
	char kname[_SEM_NAME_MAX+1];
	unsigned int namelen = 0;
	int has = 0;
	int i, j;
	/* 获取信号量的名字 */
	while (get_fs_byte(name+namelen) != '\0')
		namelen++;
	if (_SEM_NAME_MAX < namelen) {
		errno = EINVAL;
		return -1;
	}
	for (i = 0; i < namelen; i++)
		kname[i] = get_fs_byte(name+i);
	kname[i] = '\0';
	/* 在sems数组中搜索是否有该名字 */
	for (i = 0; i < _SEM_MAX; i++) {
		if (!strcmp(kname, sems[i].name)) {
			has = 1;
			break;
		}
	}
	if (!has) {
			return -1;
	}
	sems[i].name[0] = '\0';
	return 0;
}
