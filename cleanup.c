#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

typedef struct {
    long mtype;
    int terminate;
} Message;

int main() {
    Message message;
    char input;

    key_t key = ftok(".", 527);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    message.mtype = 5270;
    message.terminate = 1;

    while (1) {
        printf("Do you want the Air Traffic Control System to terminate?(Y for Yes and N for No)\n");
        scanf(" %c", &input);

        if (input == 'Y' || input == 'y') {
            if (msgsnd(msgid, &message, sizeof(message), 0) == -1) {
                perror("msgsnd failed");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }

    return 0;
}