#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#define MAX_PLANES 15
#define MAX_AIRPORTS 15
#define TERMINATION_MTYPE 5270

typedef struct
{
    long mtype;
    int takeOff;
} TakeOffMessage;

typedef struct
{
    long mtype;
    int deboardingComplete;
} DeboardingMessage;

typedef struct
{
    long mtype;
    int plane_id;
    int plane_type;
    int total_weight;
    int num_passengers;
    int departure_airport;
    int arrival_airport;
    int terminate;
} Plane;

typedef struct
{
    long mtype;
    int terminate;
} TerminateMessage;

int main()
{
    int num_airports;
    printf("Enter the number of airports to be handled/managed: ");
    scanf("%d", &num_airports);

    Plane plane;
    TerminateMessage terMessage;
    int startTermination = 0;
    int toBreak = 0;
    FILE *file;

    key_t key = ftok(".", 527);
    int msgid = msgget(key, 0666 | IPC_CREAT);

    if (msgid == -1)
    {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    Plane planes[MAX_PLANES + 1];

    file = fopen("AirTrafficController.txt", "a");
    if (file == NULL)
    {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    int requestedDeparture[MAX_PLANES + 1] = {0};
    int hasDeparted[MAX_PLANES + 1] = {0};
    int hasArrived[MAX_PLANES + 1] = {0};
    int hasSentDepartedMsg[MAX_PLANES + 1] = {0};
    int hasSentArrivalMsg[MAX_PLANES + 1] = {0};

    DeboardingMessage dbmsg;
    TakeOffMessage tkoffmsg;

    while (1)
    {

        // Check if termination message has been received and start termination if it has
        if (msgrcv(msgid, &terMessage, sizeof(terMessage), TERMINATION_MTYPE, IPC_NOWAIT) != -1)
        {
            startTermination = 1;
        }

        for (int i = 1; i <= MAX_PLANES; i++)
        {
            // check if the ith plane wants to depart
            if (msgrcv(msgid, &planes[i], sizeof(planes[i]), i, IPC_NOWAIT) != -1)
            {
                if (!startTermination)
                {
                    printf("plane %d asked to depart\n", i);
                    requestedDeparture[i] = 1;
                }
                else
                {
                    printf("plane %d asked to depart but ATC has started termination process\n", i);
                    planes[i].terminate = 1;
                    planes[i].mtype = i + 10;
                    if (msgsnd(msgid, &planes[i], sizeof(planes[i]), 0) == -1)
                    {
                        perror("msgsnd failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            else if (errno != ENOMSG)
            {
                perror("msgrcv failed");
                exit(EXIT_FAILURE);
            }
        }

        for (int i = 1; i <= MAX_PLANES; i++)
        {
            if (requestedDeparture[i] == 0)
            {
                continue;
            }
            else
            {
                // If the plane has requested to depart and not informed the departure airport earlier, inform the departure airport
                if (!hasSentDepartedMsg[i])
                {
                    // printf("ATC sent departure message for plane %d\n", i);
                    hasSentDepartedMsg[i] = 1;
                    planes[i].mtype = i + 40 + (planes[i].departure_airport - 1) * 10;
                    if (msgsnd(msgid, &planes[i], sizeof(planes[i]), 0) == -1)
                    {
                        perror("msgsnd failed");
                        exit(EXIT_FAILURE);
                    }
                }

                // Non-blocking receive to know whether plane has taken off from departure airport
                if (msgrcv(msgid, &tkoffmsg, sizeof(tkoffmsg), i + 20, IPC_NOWAIT) != -1)
                {
                    // printf("plane %d departed message received from airport\n", i);
                    if (tkoffmsg.takeOff == 1)
                    {
                        fprintf(file, "Plane %d has departed from Airport %d and will land at Airport %d.\n",
                                planes[i].plane_id,
                                planes[i].departure_airport,
                                planes[i].arrival_airport);
                        hasDeparted[i] = 1;
                    }
                }

                // If the plane has departed, inform the arrival airport
                if (hasDeparted[i])
                {
                    if (!hasSentArrivalMsg[i])
                    {
                        hasSentArrivalMsg[i] = 1;
                        planes[i].mtype = i + 140 + (planes[i].arrival_airport - 1) * 10;
                        if (msgsnd(msgid, &planes[i], sizeof(planes[i]), 0) == -1)
                        {
                            perror("msgsnd failed");
                            exit(EXIT_FAILURE);
                        }
                        // printf("ATC sent message that plane %d has departed\n", i);
                    }

                    // Non-blocking receive for landing and deboarding/unloading complete message from arrival airport
                    if (msgrcv(msgid, &dbmsg, sizeof(dbmsg), i + 30, IPC_NOWAIT) != -1)
                    {
                        // printf("plane %d arrived at the arrival airport\n", i);
                        hasArrived[i] = 1;
                    }
                }

                if (hasArrived[i])
                {
                    planes[i].mtype = i + 10;
                    if (msgsnd(msgid, &planes[i], sizeof(planes[i]), 0) == -1)
                    {
                        perror("msgsnd failed");
                        exit(EXIT_FAILURE);
                    }
                    // printf("ATC sent deboarding complete message to plane %d\n", i);

                    // Reset the plane's status
                    hasDeparted[i] = 0;
                    hasArrived[i] = 0;
                    hasSentDepartedMsg[i] = 0;
                    hasSentArrivalMsg[i] = 0;
                    requestedDeparture[i] = 0;
                }
            }
        }

        if (startTermination)
        {
            toBreak = 1;
            for (int i = 0; i < MAX_PLANES; i++)
            {
                if (requestedDeparture[i])
                {
                    toBreak = 0;
                    break;
                }
            }
        }
        if (toBreak) // if all planes have been handled and termination has to be done
        {
            break;
        }
    }

    for (int i = 1; i <= MAX_AIRPORTS; i++)
    {
        // send terminate message to all airports
        terMessage.mtype = i + 250;
        if (msgsnd(msgid, &terMessage, sizeof(terMessage), 0) == -1)
        {
            perror("msgsnd failed");
            exit(EXIT_FAILURE);
        }
    }

    // wait for all airports to send termination complete message
    for (int i = 1; i <= num_airports; i++)
    {
        if (msgrcv(msgid, &terMessage, sizeof(terMessage), i + 270, 0) == -1)
        {
            perror("msgrcv failed");
            exit(EXIT_FAILURE);
        }
    }

    if (fclose(file) == EOF)
    {
        perror("fclose failed");
        exit(EXIT_FAILURE);
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}