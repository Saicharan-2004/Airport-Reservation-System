#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>

#define MAX_THREADS 1000
#define MAX_PLANES 100

pthread_t threads[MAX_THREADS];
int thread_count = 0;

typedef struct
{
    int loadCapacity;
    int isAvailable;
    pthread_mutex_t mutex;
} Runway;

typedef struct
{
    long mtype;
    int takeOff;
} TakeOffMessage;

typedef struct
{
    long mtype;
    int terminate;
} TerminateMessage;

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

int currentPlanes[MAX_PLANES + 1] = {0};

typedef struct
{
    int airportNumber;
    int numberOfRunways;
    Runway runways[11];
} AirportDetails;

AirportDetails airportDetails;

void *handlePlane(void *arg)
{
    Plane *plane = (Plane *)arg;
    int bestFitIndex = -1;
    int overWeight = 1;
    int minDiff = INT_MAX;

    // find the best fit runway and determine if its overweight
    for (int i = 0; i < airportDetails.numberOfRunways; i++)
    {
        if (plane->total_weight < airportDetails.runways[i].loadCapacity)
        {
            int diff = airportDetails.runways[i].loadCapacity - plane->total_weight;
            if (diff < minDiff)
            {
                bestFitIndex = i;
                minDiff = diff;
            }
            overWeight = 0;
        }
    }

    if (overWeight)
    {
        // assign the default runway
        bestFitIndex = airportDetails.numberOfRunways;
        pthread_mutex_lock(&airportDetails.runways[bestFitIndex].mutex); // blocking call to lock the backup runway
    }
    else
    {
        // assign the best fit runway
        pthread_mutex_lock(&airportDetails.runways[bestFitIndex].mutex); // blocking call to lock the best fit runway
    }

    if (plane->arrival_airport == airportDetails.airportNumber)
    {
        sleep(2); // Simulate deboarding/unloading
        sleep(3); // Simulate deboarding/unloading
        printf("Plane %d has landed on Runway No. %d of Airport No. %d\n", plane->plane_id, bestFitIndex + 1, airportDetails.airportNumber);
    }
    else
    {
        sleep(3); // Boarding
        sleep(2); // Takeoff
        printf("Plane %d has completed boarding/loading on Runway No. %d of Airport No. %d\n", plane->plane_id, bestFitIndex + 1, airportDetails.airportNumber);
    }

    pthread_mutex_unlock(&airportDetails.runways[bestFitIndex].mutex); // unlock the runway
}

void *handleArrival(void *arg)
{
    Plane *plane = (Plane *)arg;
    key_t key = ftok(".", 527);
    int msgid = msgget(key, 0666);

    sleep(30); // simulate flight time
    handlePlane(plane);

    DeboardingMessage msg;
    msg.mtype = plane->plane_id + 30;
    msg.deboardingComplete = 1;

    if (msgsnd(msgid, &msg, sizeof(msg), 0) == -1) // Send deboarding complete message to ATC
    {
        perror("msgsnd failed");
        exit(1);
    }

    // printf("Sent deboarding complete message to ATC for plane %d\n", plane->plane_id);

    currentPlanes[plane->plane_id] = 0;
    pthread_exit(NULL);
}

void *handleDeparture(void *arg)
{
    Plane *plane = (Plane *)arg;
    // Plane *plane = &airport->plane;
    key_t key = ftok(".", 527);
    int msgid = msgget(key, 0666);

    // Handle takeoff
    handlePlane(plane);

    TakeOffMessage msg;
    msg.mtype = plane->plane_id + 20;
    msg.takeOff = 1;

    if (msgsnd(msgid, &msg, sizeof(msg), 0) != -1) // Send takeoff complete message to ATC
    {
        // printf("Sent takeoff complete message to ATC for plane %d\n", plane->plane_id);
    }
    else
    {
        perror("msgsnd failed");
        exit(1);
    }

    currentPlanes[plane->plane_id] = 0;
    pthread_exit(NULL);
}

int main()
{
    // Initialize airport
    printf("Enter Airport Number: ");
    scanf("%d", &airportDetails.airportNumber);

    printf("Enter number of Runways: ");
    scanf("%d", &airportDetails.numberOfRunways);

    // Initialize runways
    printf("Enter  loadCapacity  of  Runways  (give  as  a  space  separated  list  in  a single line):");
    for (int i = 0; i < airportDetails.numberOfRunways; i++)
    {
        scanf("%d", &airportDetails.runways[i].loadCapacity);
        pthread_mutex_init(&airportDetails.runways[i].mutex, NULL); // Initialize mutex
    }

    // Initialize backup runway
    airportDetails.runways[airportDetails.numberOfRunways].loadCapacity = 15000;             // Set load capacity
    pthread_mutex_init(&airportDetails.runways[airportDetails.numberOfRunways].mutex, NULL); // Initialize mutex

    int toBreak = 0;
    TerminateMessage tmsg;
    int terminateAirport = 0;
    key_t key = ftok(".", 527);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    int temp_plane_id;
    Plane planes[MAX_PLANES + 1];
    int messageReceived[MAX_PLANES + 1] = {0};

    while (1)
    {
        if (msgrcv(msgid, &tmsg, sizeof(tmsg), airportDetails.airportNumber + 250, IPC_NOWAIT) != -1)
        {
            terminateAirport = 1;
        }

        int mtype;
        for (mtype = 41 + 10 * (airportDetails.airportNumber - 1); mtype <= 40 + 10 * airportDetails.airportNumber; mtype++)
        {
            temp_plane_id = mtype % 10 == 0 ? 10 : mtype % 10;
            if (msgrcv(msgid, &planes[temp_plane_id], sizeof(planes[temp_plane_id]), mtype, IPC_NOWAIT) != -1)
            {
                // printf("Received message from air traffic controller for plane %d\n", temp_plane_id);
                messageReceived[temp_plane_id] = 1;
                break;
            }
        }

        for (mtype = 141 + 10 * (airportDetails.airportNumber - 1); mtype <= 140 + 10 * airportDetails.airportNumber; mtype++)
        {
            temp_plane_id = mtype % 10 == 0 ? 10 : mtype % 10;
            if (msgrcv(msgid, &planes[temp_plane_id], sizeof(planes[temp_plane_id]), mtype, IPC_NOWAIT) != -1)
            {
                // printf("Received message from air traffic controller for plane %d\n", temp_plane_id);
                messageReceived[temp_plane_id] = 1;
                break;
            }
        }

        for (int i = 1; i <= MAX_PLANES; i++)
        {
            if (messageReceived[i])
            {

                if (thread_count >= MAX_THREADS)
                {
                    fprintf(stderr, "Error: too many threads\n");
                    exit(1);
                }
                pthread_t thread;
                if (planes[i].mtype > 40 + 10 * (airportDetails.airportNumber - 1) && planes[i].mtype <= 40 + 10 * airportDetails.airportNumber) // the plane wants to depart
                {
                    pthread_create(&threads[thread_count++], NULL, handleDeparture, (void *)&planes[i]);
                    currentPlanes[mtype % 10 == 0 ? 10 : mtype % 10] = 1;
                }
                else if (planes[i].mtype > 140 + 10 * (airportDetails.airportNumber - 1) && planes[i].mtype <= 140 + 10 * airportDetails.airportNumber) // the plane wants to arrive
                {
                    currentPlanes[mtype % 10 == 0 ? 10 : mtype % 10] = 1;
                    pthread_create(&threads[thread_count++], NULL, handleArrival, (void *)&planes[i]);
                }
                messageReceived[i] = 0;
            }
        }

        // check if all planes have landed and taken off and terminate if needed
        if (terminateAirport)
        {
            toBreak = 1;
            for (int i = 0; i < MAX_PLANES; i++)
            {
                if (currentPlanes[i])
                {
                    toBreak = 0;
                    break;
                }
            }
        }
        if (toBreak)
        {
            break;
        }
    }

    // wait for all threads to finish
    for (int i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Cleanup
    for (int i = 0; i < airportDetails.numberOfRunways; i++)
    {
        pthread_mutex_destroy(&airportDetails.runways[i].mutex);
    }

    // Send termination message to airtrafficcontroller
    tmsg.terminate = 1;
    tmsg.mtype = airportDetails.airportNumber + 270;
    msgsnd(msgid, &tmsg, sizeof(tmsg), 0);

    return 0;
}