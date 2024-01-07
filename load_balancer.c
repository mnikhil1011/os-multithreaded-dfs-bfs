#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

#define MSG_SIZE 128

struct msg_buffer
{
    long id;
    long client_id;
    char text[MSG_SIZE];
    int choice;
};
void cleanup(int msgqid)
{
    if (msgctl(msgqid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl");
        exit(1);
    }
}
int main()
{
    struct msg_buffer msg;
    key_t msg_queue_key = ftok("lb.c", 'A');
    int msg_queue_id = msgget(msg_queue_key, 0666 | IPC_CREAT);
    if (msg_queue_id == -1)
    {
        perror("msgget");
        exit(1);
    }
    int set_id = 301; // used for identifying sec server 1 and 2

    while (1)
    {

        msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 200, 0);
        if (msg.choice == 1 || msg.choice == 2)
        {
            msg.id = 300;
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error in sending");
                exit(1);
            }
        }
        else if (msg.choice == 3 || msg.choice == 4)
        {
            msg.id = (msg.client_id % 2 == 0) ? 301 : 302;
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error in sending");
                exit(1);
            }
        }
        else if (msg.choice == 251)
        {
            msg.id = 501;
            msg.choice = set_id;
            set_id++;
            if (set_id == 303)
            {
                set_id = 301;
            }
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error in sending");
                exit(1);
            }
        }

        else if (msg.choice == 1000)
        {
            msg.id = 300;
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error in sending");
            }

            msg.id = 301;
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error in sending");
            }

            msg.id = 302;
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error in sending");
            }
            sleep(5);
            printf("Terminating load balancer....\n");
            printf("Load balancer terminated\n");
            cleanup(msg_queue_id);
            exit(0);
        }
    }
}
