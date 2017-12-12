#ifndef _SEM_H
#define _SEM_H

#include <linux/myqueue.h>

#define _SEM_MAX	20
#define _SEM_NAME_MAX	100

typedef struct {
	char name[_SEM_NAME_MAX+1];	/* semphore name string */
	int value;	/* semaphore vaule */
	unsigned int cnt;	/* semaphore referring count */
	queue squeue;	/* sleep queue on semaphore */
	struct task_struct *queue;	/* option queue for system sleep_on and wake_up */
} sem_t;

extern sem_t sems[_SEM_MAX];

/* Value returned if `sem_open` failed. */
#define SEM_FAILED 		((sem_t *)0)

#endif /* _SEM_H */
