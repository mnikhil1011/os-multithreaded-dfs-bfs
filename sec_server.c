// to compile gcc ss.c shared.c -o ss.out

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>
#include <sys/shm.h>
#include "queue.c"
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define MSG_SIZE 128
#define MAX_THREADS 100

extern int sharedVariable[31];
pthread_t threads[MAX_THREADS];
int thread_count = 0;

struct msg_buffer
{
    long id;
    long client_id;
    char text[MSG_SIZE];
    int choice;
};

struct info
{
    int n;
    int mat[30][30];
};

struct databfs
{
    int n;
    int mat[30][30];
    int vis[30];
    struct queue q;
    pthread_mutex_t lock;
};

struct everybfs
{
    struct databfs *ptr;
    int i;
};

void *tbfs(void *arg)
{
    struct everybfs *e = (struct everybfs *)arg;
    int i = e->i;
    struct databfs *dptr = e->ptr;
    for (int j = 0; j < dptr->n; j++)
    {
        if ((dptr->mat[i][j] == 1) && (dptr->vis[j] == 0))
        {
            dptr->vis[j] = 1;
            pthread_mutex_lock(&dptr->lock);
            qpush(&dptr->q, j);
            pthread_mutex_unlock(&dptr->lock);
        }
    }
    return NULL;
}

void *bfs(void *arg)
{

    struct msg_buffer *msg = (struct msg_buffer *)arg;

    // sem for readers writers
    sem_t *semaphore1 = sem_open(msg->text, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (semaphore1 == SEM_FAILED)
    {
        if (errno == EEXIST)
        {
            // The semaphore already exists, open it
            semaphore1 = sem_open(msg->text, 0);
            if (semaphore1 == SEM_FAILED)
            {
                perror("Error opening existing named mutex");
                return NULL;
            }
        }
        else
        {
            perror("Error creating named mutex");
            return NULL;
        }
    }

    // used to track number of readers in graph graphnum and open seamaphore for readers
    int graphnum = 0;
    graphnum = msg->text[1] - '0';
    if (msg->text[2] != '.')
    {
        graphnum *= 10;
        graphnum += msg->text[2] - '0';
    }

    char numberString[12]; // Array to hold the string

    // Convert the integer to a string
    sprintf(numberString, "%d", graphnum);
    sem_t *semaphoreR = sem_open(numberString, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (semaphoreR == SEM_FAILED)
    {
        if (errno == EEXIST)
        {
            // The semaphore already exists, open it
            semaphoreR = sem_open(numberString, 0);
            if (semaphoreR == SEM_FAILED)
            {
                perror("Error opening existing named mutex");
                return NULL;
            }
        }
        else
        {
            perror("Error creating named mutex");
            return NULL;
        }
    }

    char fname[MSG_SIZE];
    strcpy(fname, msg->text);

    sem_wait(semaphoreR);
        sharedVariable[graphnum]++;
        if (sharedVariable[graphnum] == 1)
        {
            sem_wait(semaphore1);
        }
    sem_post(semaphoreR);

    FILE *file = fopen(msg->text, "r");

    key_t shm_key = ftok("lb.c", (int)msg->client_id);
    int shm_id = shmget(shm_key, sizeof(struct info), 0666);

    if (shm_id == -1)
    {
        perror("shmget");
        exit(1);
    }
    struct info *info_ptr = (struct info *)shmat(shm_id, NULL, 0);

    struct databfs d1;
    fscanf(file, "%d", &d1.n);
    int n = d1.n;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            fscanf(file, "%d", &d1.mat[i][j]);
        }
    }
    for (int i = 0; i < n; i++)
    {
        d1.vis[i] = 0;
    }
    fclose(file);

    sem_wait(semaphoreR);
        sharedVariable[graphnum]--;
        if (sharedVariable[graphnum] == 0)
        {
            sem_post(semaphore1);
        }
    sem_post(semaphoreR);

    sem_close(semaphore1);
    sem_unlink(fname);
    sem_close(semaphoreR);
    sem_unlink(numberString);

    d1.q.back = 0;
    d1.q.front = 0;

    d1.vis[info_ptr->n-1] = 1;
    qpush(&d1.q, info_ptr->n-1);
    int retarr[n];
    int locn = 0;
    pthread_mutex_init(&d1.lock, NULL);
    int tot_nodes_vis=0;

    while (!qempty(&d1.q))
    {
        int k = qsize(&d1.q);

        struct everybfs e[n];
        pthread_t threads[n];
        int use[n];
        for (int i = 0; i < n; i++)
        {
            use[i] = 0;
        }

        while (k--)
        {
            tot_nodes_vis++;
            int node = qfront(&d1.q);
            retarr[locn++] = node;
            use[node] = 1;
            e[node].i = node;
            e[node].ptr = &d1;
            qpop(&d1.q);

            pthread_create(&threads[node], NULL, tbfs, &e[node]);
        }
        for (int j = 0; j < n; j++)
        {
            if (use[j])
                pthread_join(threads[j], NULL);
        }
    }
    pthread_mutex_destroy(&d1.lock);

    for (int i = 0; i < tot_nodes_vis; i++)
    {
        printf("%d  ", retarr[i]+1);
    }
    printf("\n");

    msg->id = msg->client_id;
    memset(msg->text, '\0', MSG_SIZE);

    int offset = 0;
    for (int i = 0; i < tot_nodes_vis; ++i)
    {
        // Use sprintf to convert integer to string and append to the resultString
        offset += sprintf(msg->text + offset, "%d", retarr[i]+1);
        if (i < n - 1)
        {
            // Add a comma between integers
            msg->text[offset++] = ' ';
        }
    }
    msg->text[offset] = '\0';

    int msg_queue_id = msgget(ftok("lb.c", 'A'), 0666);
    if (msgsnd(msg_queue_id, msg, sizeof(*msg) - sizeof(long), 0) == -1)
    {
        perror("error in sending");
        exit(1);
    }
}


struct datadfs
{
    int n;
    int mat[30][30];
    int vis[30];
    int posn;
    int ret[30];
    pthread_mutex_t lock;
};

struct everydfs
{
    struct datadfs *ptr;
    int i;
};

void *tdfs(void *arg)
{
    struct everydfs *e1 = (struct everydfs *)arg;
    printf("%d  ", e1->i+1);
    struct datadfs *data1 = e1->ptr;
    pthread_t thread[data1->n];
    struct everydfs esend[data1->n];
    int use[data1->n];
    int flag = 0;
    for (int j = 0; j < data1->n; j++)
    {
        esend[j].i = j;
        use[j] = 0;
        esend[j].ptr = data1;
        if ((data1->mat[e1->i][j] == 1) && (data1->vis[j] == 0))
        {
            flag = 1;
            data1->vis[j] = 1;
            use[j] = 1;
            pthread_create(&thread[j], NULL, tdfs, &esend[j]);
        }
    }
    if (flag == 0)
    {
        pthread_mutex_lock(&data1->lock);
        data1->ret[data1->posn] = e1->i;
        data1->posn += 1;
        pthread_mutex_unlock(&data1->lock);
    }

    for (int j = 0; j < data1->n; j++)
    {
        if (use[j])
            pthread_join(thread[j], NULL);
    }
}

void *dfs(void *arg)
{

    struct msg_buffer *msg = (struct msg_buffer *)arg;

    sem_t *semaphore1 = sem_open(msg->text, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (semaphore1 == SEM_FAILED)
    {
        if (errno == EEXIST)
        {
            // The semaphore already exists, open it
            semaphore1 = sem_open(msg->text, 0);
            if (semaphore1 == SEM_FAILED)
            {
                perror("Error opening existing named mutex");
                return NULL;
            }
        }
        else
        {
            perror("Error creating named mutex");
            return NULL;
        }
    }

    int graphnum = 0;
    graphnum = msg->text[1] - '0';
    if (msg->text[2] != '.')
    {
        graphnum *= 10;
        graphnum += msg->text[2] - '0';
    }

    char numberString[12]; // Array to hold the string

    // Convert the integer to a string
    sprintf(numberString, "%d", graphnum);
    sem_t *semaphoreR = sem_open(numberString, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (semaphoreR == SEM_FAILED)
    {
        if (errno == EEXIST)
        {
            // The semaphore already exists, open it
            semaphoreR = sem_open(numberString, 0);
            if (semaphoreR == SEM_FAILED)
            {
                perror("Error opening existing named mutex");
                return NULL;
            }
        }
        else
        {
            perror("Error creating named mutex");
            return NULL;
        }
    }

    char fname[MSG_SIZE];
    strcpy(fname, msg->text);

    sem_wait(semaphoreR);
        sharedVariable[graphnum]++;
        if (sharedVariable[graphnum] == 1)
        {
            sem_wait(semaphore1);
        }
    sem_post(semaphoreR);

    FILE *file = fopen(msg->text, "r");

    key_t shm_key = ftok("lb.c", (int)msg->client_id);
    int shm_id = shmget(shm_key, sizeof(struct info), 0666);

    if (shm_id == -1)
    {
        perror("shmget");
        exit(1);
    }
    struct info *info_ptr = (struct info *)shmat(shm_id, NULL, 0);

    struct datadfs d1;
    fscanf(file, "%d", &d1.n);
    int n = d1.n;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            fscanf(file, "%d", &d1.mat[i][j]);
        }
    }
    for (int i = 0; i < n; i++)
    {
        d1.vis[i] = 0;
    }
    d1.posn = 0;

    pthread_t thread;
    struct everydfs e;
    e.i = info_ptr->n-1;
    e.ptr = &d1;
    e.ptr->vis[e.i] = 1;
    pthread_mutex_init(&d1.lock, NULL);
    pthread_create(&thread, NULL, tdfs, &e);

    pthread_join(thread, NULL);

    fclose(file);
    pthread_mutex_destroy(&d1.lock);

    sem_wait(semaphoreR);
    sharedVariable[graphnum]--;
        if (sharedVariable[graphnum] == 0)
        {
            sem_post(semaphore1);
        }
    sem_post(semaphoreR);

    sem_close(semaphore1);
    sem_unlink(fname);
    sem_close(semaphoreR);
    sem_unlink(numberString);

    // msg_buffer sendmsg;
    msg->id = msg->client_id;
    memset(msg->text, '\0', MSG_SIZE);

    int offset = 0;
    for (int i = 0; i < d1.posn; ++i)
    {
        // Use sprintf to convert integer to string and append to the resultString
        offset += sprintf(msg->text + offset, "%d", d1.ret[i]+1);
        if (i < d1.posn - 1)
        {
            // Add a comma between integers
            msg->text[offset++] = ' ';
        }
    }
    msg->text[offset] = '\0';
    printf("\n");

    int msg_queue_id = msgget(ftok("lb.c", 'A'), 0666);
    if (msgsnd(msg_queue_id, msg, sizeof(*msg) - sizeof(long), 0) == -1)
    {
        perror("error in sending");
        exit(1);
    }
}

int main()
{
    for (int i = 0; i < 21; i++)
    {
        sharedVariable[i] = 0;
    }

    key_t msg_queue_key = ftok("lb.c", 'A');
    int msg_queue_id = msgget(msg_queue_key, 0666);

    if (msg_queue_id == -1)
    {
        perror("msgget");
        exit(1);
    }

    // to get even or odd
    struct msg_buffer msg1;
    msg1.choice = 251;
    msg1.id = 200;
    if (msgsnd(msg_queue_id, &msg1, sizeof(msg1) - sizeof(long), 0) == -1)
    {
        perror("error in sending");
    }
    int x = 501;
    msgrcv(msg_queue_id, &msg1, sizeof(msg1) - sizeof(long), x, 0);
    x = msg1.choice;

    while (1)
    {
        struct msg_buffer msg;
        msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), x, 0);
        printf("this server is computing the request \n");

        if (msg.choice == 1000)
        {
            // Wait for all ongoing threads to finish
            for (int i = 0; i < thread_count; ++i)
            {
                if (pthread_join(threads[i], NULL) != 0)
                {
                    perror("pthread_join");
                    return 1;
                }
            }
            // Terminate the process after all threads have finished
            printf("Terminating secondary server \n");
            printf("Secondary server terminated\n");
            exit(0);
        }
        else if (msg.choice == 3)
        {
            // dfs
            if (pthread_create(&threads[thread_count++], NULL, dfs, &msg) != 0)
            {
                perror("pthread_create");
                return 1;
            }
        }
        else if (msg.choice == 4)
        {
            if (pthread_create(&threads[thread_count++], NULL, bfs, &msg) != 0)
            {
                perror("pthread_create");
                return 1;
            }
        }
    }

    return 0;
}
