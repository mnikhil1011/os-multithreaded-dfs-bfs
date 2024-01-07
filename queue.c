#include <stdio.h>
#include <stdlib.h>

struct queue
{
    int front;
    int back;
    int q[31];
};

struct queue makequeue()
{
    struct queue q;
    q.front = 0;
    q.back = 0;
    return q;
}

int qempty(struct queue *q)
{
    return q->back == q->front;
}

int qfront(struct queue *q)
{
    return q->q[q->front];
}

void qpop(struct queue *q)
{
    q->front++;
    q->front %= 31;
}

void qpush(struct queue *q, int x)
{
    q->q[q->back] = x;
    q->back += 1;
    q->back %= 31;
}

int qsize(struct queue *q)
{
    return (q->back - q->front);
}
