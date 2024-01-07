#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define MSG_SIZE 128
#define MAX_THREADS 100

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

pthread_t threads[MAX_THREADS];
int thread_count = 0;

void *handle(void *arg)              // 
{
    struct msg_buffer *msg = (struct msg_buffer *)arg;

    msg->id = msg->client_id; // send message to client

    key_t shm_key = ftok("lb.c", msg->client_id);
    int shm_id = shmget(shm_key, sizeof(struct info), 0666);
    if (shm_id == -1)      
    {
        perror("shmget");
        exit(1);
    }
 
    struct info *info_ptr = (struct info *)shmat(shm_id, NULL, 0);
 
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

    char fname[MSG_SIZE]; // used to close seamaphore later
    strcpy(fname, msg->text);
    sem_wait(semaphore1);
    //sleep(10);

    FILE *file = fopen(msg->text, "w");
    if (file == NULL)
    {
        perror("fopen");
        exit(1);
    }
    int n = info_ptr->n;
    fprintf(file, "%d \n", n);

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            fprintf(file, "%d ", info_ptr->mat[i][j]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
    sem_post(semaphore1);
    sem_close(semaphore1);
    
    sem_unlink(fname);
    if (msg->choice == 1)
        strcpy(msg->text, "File successfully added.");
    else
        strcpy(msg->text, "File successfully modified.");
    printf("%s \n", msg->text);

    int msg_queue_id = msgget(ftok("lb.c", 'A'), 0666);
    if (msgsnd(msg_queue_id, msg, sizeof(*msg) - sizeof(long), 0) == -1)
    {
        perror("error in sending");
        exit(1);
    }
}

int main()
{

    key_t msg_queue_key = ftok("lb.c", 'A');
    int msg_queue_id = msgget(msg_queue_key, 0666);

    if (msg_queue_id == -1)
    {
        perror("msgget");
        exit(1);
    }

    while (1)
    {
        struct msg_buffer msg;
        msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 300, 0);

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
            printf("Terminating primary server\n");

            printf("Primary server terminated\n");
            exit(0);
        }
        else if (msg.choice == 1 || msg.choice == 2)
        {
            if (pthread_create(&threads[thread_count++], NULL, handle, &msg) != 0)
            {
                perror("pthread_create");
                return 1;
            }
        }
    }

    return 0;
}
