#ifndef _MYQUEUE
#define _MYQUEUE

#include <linux/sched.h>

#define QUEUE_LEN 	64

typedef struct task_struct* queue_t;

typedef struct _queue{
	queue_t q[QUEUE_LEN+1];
	int head;
	int tail;
	int size;
	int (*qempty)(struct _queue *);
	int (*qfull)(struct _queue *);
	int (*enqueue)(struct _queue *, queue_t);
	int (*dequeue)(struct _queue *, queue_t *);
} queue;

static int qempty(queue *q);
static int qfull(queue *q);
extern int enqueue(queue *q, queue_t x);
extern int dequeue(queue *q, queue_t *x);
extern void initqueue(queue *q);

#endif /* _MYQUEUE */
