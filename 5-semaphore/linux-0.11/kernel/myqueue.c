#include <linux/myqueue.h>

int qempty(queue * q) {
	if (q->head == q->tail)
		return 1;
	else
		return 0;
}

int qfull(queue *q) {
	if (q->head == ((q->tail + 1) % q->size))
		return 1;
	else
		return 0;
}

int enqueue(queue *q, queue_t x) {
	if (q->qfull(q))
		return -1;
	q->q[q->tail] = x;
	q->tail = (q->tail + 1) % q->size;
	return 0;
}

int dequeue(queue *q, queue_t *x) {
	if (q->qempty(q))
		return -1;
	*x = q->q[q->head];
	q->head = (q->head + 1) % q->size;
	return 0;
}

void initqueue(queue *q) {
	q->head = 0;
	q->tail = 0;
	q->size = QUEUE_LEN + 1;
	q->qfull = qfull;
	q->qempty = qempty;
	q->enqueue = enqueue;
	q->dequeue = dequeue;
}
