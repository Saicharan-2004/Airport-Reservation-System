#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_PASSENGERS 15
#define MAX_CARGO_ITEMS 150
#define CREW_WEIGHT 75
#define MAX_AIRPORTS 15

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

int main()
{
    int plane_id, plane_type, num_seats, num_cargo_items, avg_cargo_weight;
    int departure_airport, arrival_airport;
    int total_weight = 0;
    int i, luggage_weight, body_weight;

    pid_t pid;

    printf("Enter Plane ID: ");
    scanf("%d", &plane_id);

    printf("Enter Type of Plane: ");
    scanf("%d", &plane_type);

    if (plane_type == 1) // Passenger Plane
    {
        printf("Enter Number of Occupied Seats: ");
        scanf("%d", &num_seats);

        for (i = 0; i < num_seats; i++)
        {
            int pipefd[2];
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            pid = fork();
            if (pid == -1)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if (pid == 0)
            {
                close(pipefd[0]);

                printf("Enter Weight of Your Luggage: ");
                scanf("%d", &luggage_weight);

                printf("Enter Your Body Weight: ");
                scanf("%d", &body_weight);

                write(pipefd[1], &luggage_weight, sizeof(int));
                write(pipefd[1], &body_weight, sizeof(int));

                close(pipefd[1]);
                exit(EXIT_SUCCESS);
            }
            else // Parent process
            {
                close(pipefd[1]);

                read(pipefd[0], &luggage_weight, sizeof(int));
                read(pipefd[0], &body_weight, sizeof(int));

                total_weight += luggage_weight + body_weight;

                close(pipefd[0]);
                wait(NULL);
            }
        }

        total_weight += 7 * CREW_WEIGHT;
    }
    else // cargo plane
    {
        printf("Enter Number of Cargo Items: ");
        scanf("%d", &num_cargo_items);

        printf("Enter Average Weight of Cargo Items: ");
        scanf("%d", &avg_cargo_weight);

        total_weight = num_cargo_items * avg_cargo_weight + 2 * CREW_WEIGHT;
    }

    printf("Enter Airport Number for Departure: ");
    scanf("%d", &departure_airport);

    printf("Enter Airport Number for Arrival: ");
    scanf("%d", &arrival_airport);

    Plane plane = {plane_id, plane_id, plane_type, total_weight, num_seats, departure_airport, arrival_airport};
    key_t key = ftok(".", 527);
    int msgid = msgget(key, 0666 | IPC_CREAT); // Including IPC_CREAT flag in case plane process is created before ATC process
    if (msgid == -1)
    {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Send a message to the air traffic controller with the plane details
    if (msgsnd(msgid, &plane, sizeof(plane), 0) == -1)
    {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }

    // Receive a message from the air traffic controller
    if (msgrcv(msgid, &plane, sizeof(plane), plane_id + 10, 0) != -1)
    {
        if (plane.terminate == 1)
        {
            printf("Plane %d requested to depart but ATC has started termination process\n", plane_id);
            return 0;
        }

        printf("Plane %d has successfully traveled from Airport %d to Airport %d!\n", plane_id, departure_airport, arrival_airport);
    }
    else
    {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }

    return 0;
}