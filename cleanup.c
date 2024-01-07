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

int main()
{
    struct msg_buffer msg;
    msg.choice = 1000;
    msg.id = 200;
    key_t msg_queue_key = ftok("lb.c", 'A');
    int msg_queue_id = msgget(msg_queue_key, 0666);
    if (msg_queue_id == -1)
    {
        perror("msgget");
        exit(1);
    }

    char input, i;
    while (1)
    {
        printf("Do you want the server to terminate? Press Y for yes, N for no :   ");
        scanf("%c", &input);
        scanf("%c", &i);// to take input of '\n' after user says Y/N 

        if (input == 'Y' || input == 'y')
        {
            if (msgsnd(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0) == -1)
            {
                perror("error");
                exit(0);
            }
            printf("Terminating cleanup process....\n");
            printf("Cleanup terminated\n");
            exit(1);
        }
        continue;
    }
}
