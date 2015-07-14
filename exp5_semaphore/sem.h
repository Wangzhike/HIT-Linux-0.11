#ifndef _SEM_H
#define _SEM_H

#include <linux/sched.h>


#define SEMTABLE_LEN	20
#define SEM_NAME_LEN    20

typedef struct semaphore{
    char name[SEM_NAME_LEN];
    int value;
    struct task_struct *queue;
} sem_t;
extern sem_t semtable[SEMTABLE_LEN];

#endif
