#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/shm.h>

#define MSG_SIZE 128

struct msg_buffer
{
    long id;// locn where we want message to go
    long client_id;
    char text[MSG_SIZE];
    int choice;
};

struct info// for shm
{
    int n;
    int mat[30][30];
};

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

        printf("\nMenu:\n");
        printf("1. Add a new graph to the database\n");
        printf("2. Modify an existing graph of the database\n");
        printf("3. Perform DFS on an existing graph of the database\n");
        printf("4. Perform BFS on an existing graph of the database\n");

        struct msg_buffer msg;
        memset(msg.text, '\0', MSG_SIZE);
        msg.id = 200;                    // send msg to lb
        printf("Enter Sequence Number: ");
        long id;
        scanf("%ld", &id);
        msg.client_id = id;
        printf("Enter Operation Number: ");
        scanf("%d", &msg.choice);
        printf("Enter Graph File Name: ");
        scanf("%s", msg.text);

        struct msqid_ds queue_info;
        if(msgctl(msg_queue_id, IPC_STAT, &queue_info) == -1)
        {
            if (errno == EINVAL) 
            {
                printf("Message queue has been removed. cant process request  Terminating.\n");
                exit(0);
            } 
            else 
            {
                perror("msgctl");
                exit(1);
            }
        }
        

        // create shm
        key_t shm_key = ftok("lb.c",msg.client_id);
        int shm_id = shmget(shm_key, sizeof(struct info), 0666 | IPC_CREAT);
        if (shm_id == -1)
        {
            perror("shmget");
            exit(1);
        }
        struct info *info_ptr = (struct info *)shmat(shm_id, NULL, 0);

        
        switch (msg.choice)
        {
        case 1:
        case 2:
            printf("Enter number of nodes of the graph : ");
            int n;
            scanf("%d",  &info_ptr->n);
            n=info_ptr->n;             
            printf("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters \n");
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    int x;
                    scanf("%d", &x);
                    info_ptr->mat[i][j] = x;
                }
            }
            printf("\n");
            break;
        case 3:
        case 4:
            printf("Enter starting vertex : ");
            scanf("%d", &info_ptr->n);
            break;

        default:
            printf("Invalid imput. Enter again\n");
            continue;
        }
        

        //struct msqid_ds queue_info;
        if(msgctl(msg_queue_id, IPC_STAT, &queue_info) == -1)
        {
            if (errno == EINVAL) 
            {
                printf("Message queue has been removed. cant process request  Terminating.\n");
                exit(0);
            } 
            else 
            {
                perror("msgctl");
                exit(1);
            }
        }


        if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
        {
            perror("error in sending");
            exit(1);
        }
        // shmdt(info_ptr);

        struct msg_buffer msg2;
        memset(msg2.text, '\0', MSG_SIZE);
        if (msgrcv(msg_queue_id, &msg2, sizeof(msg2) - sizeof(long), id, 0) == -1)
        {
            perror("error in rcv");
            exit(1);
        }
        // del shm here
        
        shmctl(shm_id, IPC_RMID, NULL);

        printf("Server reply: %s \n", msg2.text);

        
    }
}
