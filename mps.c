#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>

#define MAX_WORD_SIZE 64

// Structures
struct BurstItem {
    int pid;
    int processorId;
    long int burstLength;
    long int arrivalTime;
    long int finishTime;
    long int turnaroundTime;
    long int waitTime;
    long int remainingTime;
};

struct ListNode {
    struct BurstItem* data;
    struct ListNode* next;
};

struct QueueTableItem {
    struct ListNode* head;
    struct ListNode* tail;
    int totalLoad;
    int size;
    pthread_mutex_t mutex;
};

struct ProcessList {
    struct ListNode* head;
    struct ListNode* tail;
    int size;
    pthread_mutex_t mutex_lock;
};

struct threadArg {
	char ALG[5];
    char SAP[2];
    int Q;
    int OUTMODE;
    int threadID;
};

// Global Variables
struct QueueTableItem** queueTable = NULL;
struct ProcessList* processList = NULL;
long int startTimeMS = 0;
int isOutFile = 0;
FILE* output_fp = NULL;


// Function Definitions
void* do_task(void*);

// main function
int main(int argc, char* argv[]) {
    int numProc = 2;
    char sap[] = "M";
    char qs[] = "RM";
    char alg[5] = "RR";
    int q = 20;
    char infile[MAX_WORD_SIZE] = "in.txt";
    FILE* inputFP = NULL;
    int outmode = 1;
    char outfile[MAX_WORD_SIZE] = "out.txt";
    int T = 200, T1 = 10, T2 = 1000, L = 100, L1 = 10, L2 = 500, PC = 10; // Parameters of -r
    int isI = 0;
    int isR = 0;
    int useR = 0;

    // Initialize ProcessList for the end result
    processList = malloc(sizeof(struct ProcessList));
    processList->head = NULL;
    processList->tail = NULL;
    processList->size = 0;
    pthread_mutex_init(&(processList->mutex_lock), NULL); // Initialize the mutex
    

    // Taking the command line arguments
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) {
            numProc = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "-a")) {
            strcpy(sap, argv[i + 1]);
            strcpy(qs, argv[i + 2]);
        }
        else if (!strcmp(argv[i], "-s")) {
            strcpy(alg, argv[i + 1]);
            q = atoi(argv[i + 2]);
        }
        else if (!strcmp(argv[i], "-i")) {
            strcpy(infile, argv[i + 1]);
            isI = 1;
        }
        else if (!strcmp(argv[i], "-m")) {
            outmode = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "-o")) {
            strcpy(outfile, argv[i + 1]);
            isOutFile = 1;
        }
        else if (!strcmp(argv[i], "-r")) {
            T = atoi(argv[i + 1]);
            T1 = atoi(argv[i + 2]);
            T2 = atoi(argv[i + 3]);
            L = atoi(argv[i + 4]);
            L1 = atoi(argv[i + 5]);
            L2 = atoi(argv[i + 6]);
            PC = atoi(argv[i + 7]);
            isR = 1;
        }
    }

    // Create the output file pointer if there is an output file specified
    if (isOutFile == 1) {
        output_fp = fopen(outfile, "a");
    }

    // Decide if we use random distribution or the file input
    if (isI == 1) {
        if (isR == 1) {
            useR = 1;
        }
        else {
            useR = 0;
        }
    }
    else {
        useR = 1;
    }

    // Create and populate 2 list, one will hold the randomly created burst and the other will hold the randomly created IAT
    int randomIATList[PC - 1];
    int randomPLList[PC];
    if (useR == 1) {
        // Create
        float rate = 0;
        float u = 0;
        float x = 0;

        srand(time(NULL)); // To guarantee getting a different random number each time

        // Populate the IAT list
        for (int i = 0; i < (PC - 1); i++) {
            while (1) {
                rate = (1.0 / T);
                u = (float) rand() / RAND_MAX;
                x = ((-1) * log((1 - u))) / rate;

                if ((x >= T1) && (x <= T2)) {
                    break;
                }
            }
            // X is now defined and can be added to the IAT list
            randomIATList[i] = round(x);
        }

        // Populate the PL list
        for (int i = 0; i < PC; i++) {
            while (1) {
                rate = (1.0 / L);
                u = (float) rand() / RAND_MAX;
                x = ((-1) * log((1 - u))) / rate;

                if ((x >= L1) && (x <= L2)) {
                    break;
                }
            }
            // X is now defined and can be added to the PL list
            randomPLList[i] = round(x);
        }
        // both lists are populated here
    }

    // Queue initialization
    int numOfQueues = 0;
    if ((numProc == 1) || !strcmp(sap, "S")) {
        numOfQueues = 1;
    }
    else {
        numOfQueues = numProc;
    }

    queueTable = malloc(sizeof(struct QueueTableItem*) * numOfQueues);
    for (int i = 0; i < numOfQueues; i++) {
        queueTable[i] = malloc(sizeof(struct QueueTableItem));
        (queueTable[i])->head = NULL;
        (queueTable[i])->tail = NULL;
        (queueTable[i])->size = 0;
        (queueTable[i])->totalLoad = 0;
        pthread_mutex_init(&((queueTable[i])->mutex), NULL); // Initialize the mutex
    }

    // Thread creations
    pthread_t tids[numProc];
    struct threadArg tArgs[numProc];
    pthread_attr_t attr;
    int ret;
    for (int i = 0; i < numProc; i++) {
        pthread_attr_init(&attr);

        strcpy(tArgs[i].ALG, alg);
        strcpy(tArgs[i].SAP, sap);
        tArgs[i].Q = q;
        tArgs[i].OUTMODE = outmode;
        tArgs[i].threadID = i;

		ret = pthread_create (&(tids[i]), &attr, do_task, (void *) &(tArgs[i]));

		if (ret != 0) {
			printf("thread create failed \n");
			exit(1);
		}
    }

    if (useR == 0) {
        inputFP = fopen(infile, "r");
    }
    
    struct ListNode* current = NULL;
    char buffer[MAX_WORD_SIZE];
    int sleepDuration = 0;
    int burstLength = 0;
    int count = 0;
    int queueIndex = 0;
    struct timeval currentTime;
    int minTotalLoadLength, minTotalLoadIndex;
    long int currentTimeMS = 0;
    
    // If the burst lengths will not be generated randomly then do this
    if (useR == 0) {
        while (fscanf(inputFP, "%s", buffer) != EOF) {
            if (!strcmp(buffer, "PL")) {

                // Assign attributes to task instance
                fscanf(inputFP, "%d", &burstLength);
                gettimeofday(&currentTime, NULL);
                currentTimeMS = ((currentTime.tv_sec) * 1000) + ((currentTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond
                
                if (count == 0) {
                    startTimeMS = currentTimeMS;
                }

                current = malloc(sizeof(struct ListNode));
                current->next = NULL;

                current->data = malloc(sizeof(struct BurstItem));
                current->data->pid = count + 1;
                current->data->arrivalTime = currentTimeMS - startTimeMS;
                current->data->burstLength = burstLength;
                current->data->remainingTime = burstLength;

                if (outmode == 3) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "new burst is added to queue: pid=%d, arrivaltime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, current->data->arrivalTime, current->data->burstLength, current->data->remainingTime);
                    }
                    else {
                        printf("new burst is added to queue: pid=%d, arrivaltime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, current->data->arrivalTime, current->data->burstLength, current->data->remainingTime);
                    }
                }

                // Selection of the queue index
                // RM option
                if (!strcmp(qs, "RM")) {
                    queueIndex = (count % numOfQueues);
                }
                // LM option
                else if (!strcmp(qs, "LM")) {
                    // Find the queue with min total load and if it is same, pick up the one with smaller id
                    minTotalLoadIndex = 0;
                    minTotalLoadLength = queueTable[0]->totalLoad;
                    for (int i = 1; i < numOfQueues; i++) {
                        if (queueTable[i]->totalLoad < minTotalLoadLength) {
                            minTotalLoadLength = queueTable[i]->totalLoad;
                            minTotalLoadIndex = i;
                        }
                    }
                    queueIndex = minTotalLoadIndex;
                }
                // NA option in which the single queue approach is used and there is only one common queue
                else {
                    queueIndex = 0;
                }

                pthread_mutex_lock(&(queueTable[queueIndex]->mutex));
                /* critical section begin */

                // After the queue index is decided, put the task into the respective queue's tail
                if (queueTable[queueIndex]->size == 0) {
                    queueTable[queueIndex]->head = current;
                    queueTable[queueIndex]->tail = current;
                }

                else {
                    queueTable[queueIndex]->tail->next = current;
                    queueTable[queueIndex]->tail = current;
                }

                queueTable[queueIndex]->totalLoad += burstLength;
                (queueTable[queueIndex]->size)++;
                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueIndex]->mutex));

                count++;
            }

            else {
                fscanf(inputFP, "%d", &sleepDuration);
                usleep(sleepDuration * 1000);
            }
        }
    }

    // If the burst lengths will be generated randomly then do this
    else {
        for (int i = 0; i < PC; i++) {
            burstLength = randomPLList[i];
            if (i < (PC - 1)) {
                sleepDuration = randomIATList[i];
            }
            
            gettimeofday(&currentTime, NULL);
            currentTimeMS = ((currentTime.tv_sec) * 1000) + ((currentTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond
            
            if (count == 0) {
                startTimeMS = currentTimeMS;
            }

            current = malloc(sizeof(struct ListNode));
            current->next = NULL;

            current->data = malloc(sizeof(struct BurstItem));
            current->data->pid = count + 1;
            current->data->arrivalTime = currentTimeMS - startTimeMS;
            current->data->burstLength = burstLength;
            current->data->remainingTime = burstLength;

            if (outmode == 3) {
                if (isOutFile == 1) {
                    fprintf(output_fp, "new burst is added to queue: pid=%d, arrivaltime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, current->data->arrivalTime, current->data->burstLength, current->data->remainingTime);
                }
                else {
                    printf("new burst is added to queue: pid=%d, arrivaltime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, current->data->arrivalTime, current->data->burstLength, current->data->remainingTime);
                }
            }

            // Selection of the queue index
            // RM option
            if (!strcmp(qs, "RM")) {
                queueIndex = (count % numOfQueues);
            }
            // LM option
            else if (!strcmp(qs, "LM")) {
                // Find the queue with min total load and if it is same, pick up the one with smaller id
                minTotalLoadIndex = 0;
                minTotalLoadLength = queueTable[0]->totalLoad;
                for (int i = 1; i < numOfQueues; i++) {
                    if (queueTable[i]->totalLoad < minTotalLoadLength) {
                        minTotalLoadLength = queueTable[i]->totalLoad;
                        minTotalLoadIndex = i;
                    }
                }
                queueIndex = minTotalLoadIndex;
            }
            // NA option in which the single queue approach is used and there is only one common queue
            else {
                queueIndex = 0;
            }

            pthread_mutex_lock(&(queueTable[queueIndex]->mutex));
            /* critical section begin */

            // After the queue index is decided, put the task into the respective queue's tail
            if (queueTable[queueIndex]->size == 0) {
                queueTable[queueIndex]->head = current;
                queueTable[queueIndex]->tail = current;
            }

            else {
                queueTable[queueIndex]->tail->next = current;
                queueTable[queueIndex]->tail = current;
            }

            queueTable[queueIndex]->totalLoad += burstLength;
            (queueTable[queueIndex]->size)++;
            /* critical section end */
            pthread_mutex_unlock(&(queueTable[queueIndex]->mutex));

            count++;

            // Now simulate the IAT until the last burst
            if (i < (PC - 1)) {
                usleep(sleepDuration * 1000);
            }
        }
    }

    // Addition of the dummy items into each queue after all the tasks are added
    for (int i = 0; i < numOfQueues; i++) {
        // Dummy item population
        gettimeofday(&currentTime, NULL);
        currentTimeMS = ((currentTime.tv_sec) * 1000) + ((currentTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond

        current = malloc(sizeof(struct ListNode));
        current->next = NULL;

        current->data = malloc(sizeof(struct BurstItem));
        current->data->pid = -1; // Dummy items has pid = -1
        current->data->arrivalTime = currentTimeMS - startTimeMS;
        current->data->burstLength = 0;
        current->data->remainingTime = 0;

        pthread_mutex_lock(&(queueTable[i]->mutex));
	    /* critical section begin */

        // Dummy item insertion
        if (queueTable[i]->size == 0) {
            queueTable[i]->head = current;
            queueTable[i]->tail = current;
        }

        else {
            queueTable[i]->tail->next = current;
            queueTable[i]->tail = current;
        }

        queueTable[i]->totalLoad += burstLength;
        (queueTable[i]->size)++;

        /* critical section end */
	    pthread_mutex_unlock(&(queueTable[i]->mutex));
    }

    // Main thread waits until all threads terminate
    for (int i = 0; i < numProc; i++) {
        ret = pthread_join(tids[i], NULL);
		if (ret != 0) {
			printf("thread join failed \n");
			exit(1);
		}
    }
    // Threads terminated successfully here
    if (outmode == 3) {
        if (isOutFile == 1) {
            fprintf(output_fp, "all CPU's are done with their execution\n");
        }
        else {
            printf("all CPU's are done with their execution\n");
        }
    }

    // Print the completed tasks in ascending order
    if (outmode != 1) {
        if (isOutFile == 1) {
            fprintf(output_fp, "\n");
        }
        else {
            printf("\n");
        }
    }

    if (isOutFile == 1) {
        fprintf(output_fp, "%-12s %-12s %-15s %-10s %-13s %-18s %-17s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
    }
    else {
        printf("%-12s %-12s %-15s %-10s %-13s %-18s %-17s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime", "turnaround");
    }

    int procCount = 0;
    long int totalTurnTime = 0;
    struct ListNode* temp = NULL;
    double avgTurnTime = 0;

    while (procCount != processList->size) {
        temp = processList->head;
        while (temp != NULL) {
            if (temp->data->pid == (procCount + 1)) {
                if (isOutFile == 1) {
                    fprintf(output_fp, "%-12d %-12d %-15ld %-10ld %-13ld %-18ld %-17ld\n", temp->data->pid, temp->data->processorId, temp->data->burstLength, temp->data->arrivalTime, temp->data->finishTime, temp->data->waitTime, temp->data->turnaroundTime);
                }
                else {
                    printf("%-12d %-12d %-15ld %-10ld %-13ld %-18ld %-17ld\n", temp->data->pid, temp->data->processorId, temp->data->burstLength, temp->data->arrivalTime, temp->data->finishTime, temp->data->waitTime, temp->data->turnaroundTime);
                }
                totalTurnTime += temp->data->turnaroundTime;
                break;
            }
            temp = temp->next;
        }
        procCount++;
    }

    // Printing the avg turnaround time
    avgTurnTime = (totalTurnTime / (1.0 * (processList->size)));

    if (isOutFile == 1) {
        fprintf(output_fp, "average turnaround time: %.2f ms\n", avgTurnTime);
    }
    else {
        printf("average turnaround time: %.2f ms\n", avgTurnTime);
    }

    // close the files if they are opened
    if (isOutFile == 1) {
        fclose(output_fp);
    }
    if (useR == 0) {
        fclose(inputFP);
    }

    // Deallocation of the dynamic memory
    for (int i = 0; i < numOfQueues; i++) {
        pthread_mutex_destroy(&((queueTable[i])->mutex)); // Destroy the mutex
        free(queueTable[i]);
    }
    free(queueTable);

    pthread_mutex_destroy(&(processList->mutex_lock)); // Destroy the mutex
    struct ListNode* tempDel = processList->head;
    struct ListNode* tempPrev = NULL;
    while (tempDel != NULL) {
        tempPrev = tempDel;
        tempDel = tempDel->next;
        free(tempPrev->data);
        free(tempPrev);
    }
    free(processList);

    // Program ends here
    return 0;
}

// Function that the threads will execute
void* do_task(void* arg_ptr) {
    char alg[5];
    char sap[2];
    int outmode;
    int tid;
    int queueID;
    struct ListNode* current = NULL;
    struct ListNode* temp = NULL;
    struct ListNode* prev = NULL;
    int minBurst = 0;
    int sleepDuration = 0;
    int remainingBurstRR = 0;
    int timeQuantumRR = 0;
    struct timeval currentTime;
    struct timeval tempTime;
    struct timeval tempTime2;
    long int currentTimeMS = 0;
    long int tempTimeMS = 0;
    long int tempTimeMS2 = 0;
    long int oldRemainingTime = 0;

    strcpy(alg, ((struct threadArg*)arg_ptr)->ALG);
    strcpy(sap, ((struct threadArg*)arg_ptr)->SAP);
    timeQuantumRR = ((struct threadArg*)arg_ptr)->Q;
    outmode = ((struct threadArg*)arg_ptr)->OUTMODE;
    tid = ((struct threadArg*)arg_ptr)->threadID;

    // Queue selection
    if (!strcmp(sap, "S")) {
        queueID = 0;
    }
    else {
        queueID = tid;
    }

    // Loop through queue untill the dummy item is reached
    while (1) {
        
        // If the alg is FCFS
        if (!strcmp(alg, "FCFS")) {
            /* critical section begin */
            pthread_mutex_lock(&(queueTable[queueID]->mutex));
            current = queueTable[queueID]->head;

            // Sleep 1 millisecond if the queue is empty
            if (current == NULL) {
                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                usleep(1000);
            }
            // End the loop if the dummy item is reached
            else if (current->data->pid == -1) {
                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                break;
            }
            // We know that there is an item to pick here
            else {
                // Remove the burst from the shared queue and then release the lock
                if (queueTable[queueID]->head == queueTable[queueID]->tail) {
                    queueTable[queueID]->tail = NULL;
                }
                queueTable[queueID]->head = queueTable[queueID]->head->next;
                queueTable[queueID]->totalLoad -= current->data->burstLength;
                (queueTable[queueID]->size)--;
                current->next = NULL;

                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));

                // Display the Info
                gettimeofday(&tempTime, NULL);
                tempTimeMS = ((tempTime.tv_sec) * 1000) + ((tempTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond
                        
                if (outmode == 2) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, current->data->remainingTime);
                    }
                    else {
                        printf("time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, current->data->remainingTime);
                    }
                }

                else if (outmode == 3) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, current->data->remainingTime);
                    }
                    else {
                        printf("burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, current->data->remainingTime);
                    }
                }

                // Simulate burst with sleep
                sleepDuration = current->data->burstLength;
                usleep(sleepDuration * 1000);
                
                // Update the burst with the remaining values
                gettimeofday(&currentTime, NULL);
                currentTimeMS = ((currentTime.tv_sec) * 1000) + ((currentTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond

                current->data->finishTime = currentTimeMS - startTimeMS;
                current->data->turnaroundTime = current->data->finishTime - current->data->arrivalTime;
                current->data->waitTime = current->data->turnaroundTime - current->data->burstLength;
                current->data->remainingTime = 0;
                current->data->processorId = tid + 1;

                // Display the info for finished burst
                if (outmode == 3) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "burst has finished: pid=%d, cpu=%d, arrivaltime=%ld, finishtime=%ld, burstlen=%ld, remainingtime=%ld, waitingtime=%ld, turnaroundtime=%ld\n", current->data->pid, current->data->processorId, current->data->arrivalTime, current->data->finishTime, current->data->burstLength, current->data->remainingTime, current->data->waitTime, current->data->turnaroundTime);
                    }
                    else {
                        printf("burst has finished: pid=%d, cpu=%d, arrivaltime=%ld, finishtime=%ld, burstlen=%ld, remainingtime=%ld, waitingtime=%ld, turnaroundtime=%ld\n", current->data->pid, current->data->processorId, current->data->arrivalTime, current->data->finishTime, current->data->burstLength, current->data->remainingTime, current->data->waitTime, current->data->turnaroundTime);
                    }
                }

                // Add the burst to the final process list using mutex lock

                /* critical section begin */
                pthread_mutex_lock(&(processList->mutex_lock));
                if (processList->size == 0) {
                    processList->head = current;
                    processList->tail = current;
                }
                else {
                    processList->tail->next = current;
                    processList->tail = current;
                }
                (processList->size)++;
                /* critical section end */
                pthread_mutex_unlock(&(processList->mutex_lock));
            }
        }

        // If the alg is SJF
        else if (!strcmp(alg, "SJF")) {
            /* critical section begin */
            pthread_mutex_lock(&(queueTable[queueID]->mutex));

            temp = queueTable[queueID]->head;
            current = temp;

            // Sleep 1 millisecond if the queue is empty
            if (current == NULL) {
                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                usleep(1000);
            }
            // End the loop if the dummy item is reached
            else if (current->data->pid == -1) {
                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                break;
            }
            // We know that there is an item to pick here
            else {
                // Find the task with the smallest burst
                minBurst = temp->data->burstLength;
                while (temp != NULL) {
                    if ((temp->data->pid != -1) && (temp->data->burstLength < minBurst)) {
                        minBurst = temp->data->burstLength;
                        current = temp;
                    }
                    temp = temp->next;
                } // current has the smallest burst now

                // Find the node previous to the smallest burst
                temp = queueTable[queueID]->head;
                prev = NULL;
                while (temp != current) {
                    prev = temp;
                    temp = temp->next;
                } // prev holds the previous node of the min burst now
                
                if (prev == NULL) {
                    queueTable[queueID]->head = queueTable[queueID]->head->next;
                }
                else {
                    prev->next = current->next;
                }

                if (current == queueTable[queueID]->tail) {
                    queueTable[queueID]->tail = prev;
                }
                current->next = NULL;
                queueTable[queueID]->totalLoad -= current->data->burstLength;
                (queueTable[queueID]->size)--;

                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));

                // Display the info
                gettimeofday(&tempTime, NULL);
                tempTimeMS = ((tempTime.tv_sec) * 1000) + ((tempTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond   

                if (outmode == 2) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, current->data->remainingTime);
                    }
                    else {
                        printf("time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, current->data->remainingTime);
                    }
                }

                else if (outmode == 3) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, current->data->remainingTime);
                    }
                    else {
                        printf("burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, current->data->remainingTime);
                    }
                }

                // Simulate burst with sleep
                sleepDuration = current->data->burstLength;
                usleep(sleepDuration * 1000);

                // Update the burst with the remaining values
                gettimeofday(&currentTime, NULL);
                currentTimeMS = ((currentTime.tv_sec) * 1000) + ((currentTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond

                current->data->finishTime = currentTimeMS - startTimeMS;
                current->data->turnaroundTime = current->data->finishTime - current->data->arrivalTime;
                current->data->waitTime = current->data->turnaroundTime - current->data->burstLength;
                current->data->remainingTime = 0;
                current->data->processorId = tid + 1;

                // Display the info for finished burst
                if (outmode == 3) {
                    if (isOutFile == 1) {
                        fprintf(output_fp, "burst has finished: pid=%d, cpu=%d, arrivaltime=%ld, finishtime=%ld, burstlen=%ld, remainingtime=%ld, waitingtime=%ld, turnaroundtime=%ld\n", current->data->pid, current->data->processorId, current->data->arrivalTime, current->data->finishTime, current->data->burstLength, current->data->remainingTime, current->data->waitTime, current->data->turnaroundTime);
                    }
                    else {
                        printf("burst has finished: pid=%d, cpu=%d, arrivaltime=%ld, finishtime=%ld, burstlen=%ld, remainingtime=%ld, waitingtime=%ld, turnaroundtime=%ld\n", current->data->pid, current->data->processorId, current->data->arrivalTime, current->data->finishTime, current->data->burstLength, current->data->remainingTime, current->data->waitTime, current->data->turnaroundTime);
                    }
                }

                // Add the burst to the final process list using mutex lock

                /* critical section begin */
                pthread_mutex_lock(&(processList->mutex_lock));
                if (processList->size == 0) {
                    processList->head = current;
                    processList->tail = current;
                }
                else {
                    processList->tail->next = current;
                    processList->tail = current;
                }
                (processList->size)++;
                /* critical section end */
                pthread_mutex_unlock(&(processList->mutex_lock));
            }
        }

        // If the alg is RR
        else if (!strcmp(alg, "RR")) {
            pthread_mutex_lock(&(queueTable[queueID]->mutex));
	        /* critical section begin */
            current = queueTable[queueID]->head;
            
            // Sleep 1 millisecond if the queue is empty
            if (current == NULL) {
                /* critical section end */
                pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                usleep(1000);
            }
            // End the loop if the dummy item is reached
            else if (current->data->pid == -1) {
                if (queueTable[queueID]->size <= 1) {
                    /* critical section end */
                    pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                    break;
                }

                else {
                    // Remove the process from the head of the queue
                    queueTable[queueID]->head = queueTable[queueID]->head->next;
                    current->next = NULL;

                    // Insert the process at the tail of the queue
                    queueTable[queueID]->tail->next = current;
                    queueTable[queueID]->tail = current;

                    /* critical section end */
                    pthread_mutex_unlock(&(queueTable[queueID]->mutex));
                }
            }
            // We know that there is an item to pick here which is not the dummy item
            else {
                remainingBurstRR = current->data->remainingTime;
                if (remainingBurstRR <= timeQuantumRR) {
                    // Remove the burst from the shared queue and then release the lock
                    if (queueTable[queueID]->head == queueTable[queueID]->tail) {
                        queueTable[queueID]->tail = NULL;
                    }
                    queueTable[queueID]->head = queueTable[queueID]->head->next;
                    queueTable[queueID]->totalLoad -= remainingBurstRR;
                    (queueTable[queueID]->size)--;
                    current->next = NULL;

                    /* critical section end */
                    pthread_mutex_unlock(&(queueTable[queueID]->mutex));

                    // Display the Info
                    gettimeofday(&tempTime, NULL);
                    tempTimeMS = ((tempTime.tv_sec) * 1000) + ((tempTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond  

                    if (outmode == 2) {
                        if (isOutFile == 1) {
                            fprintf(output_fp, "time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, current->data->remainingTime);
                        }
                        else {
                            printf("time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, current->data->remainingTime);
                        }
                    }

                    else if (outmode == 3) {
                        if (isOutFile == 1) {
                            fprintf(output_fp, "burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, current->data->remainingTime, timeQuantumRR);
                        }
                        else {
                            printf("burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, current->data->remainingTime, timeQuantumRR);
                        }
                    }

                    // Simulate burst with sleep
                    usleep(remainingBurstRR * 1000);
                    
                    // Update the burst with the remaining values
                    gettimeofday(&currentTime, NULL);
                    currentTimeMS = ((currentTime.tv_sec) * 1000) + ((currentTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond

                    current->data->finishTime = currentTimeMS - startTimeMS;
                    current->data->turnaroundTime = current->data->finishTime - current->data->arrivalTime;
                    current->data->waitTime = current->data->turnaroundTime - current->data->burstLength;
                    current->data->remainingTime = 0;
                    current->data->processorId = tid + 1;

                    // Display the info for finished burst
                    if (outmode == 3) {
                        if (isOutFile == 1) {
                            fprintf(output_fp, "burst has finished: pid=%d, cpu=%d, arrivaltime=%ld, finishtime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d, waitingtime=%ld, turnaroundtime=%ld\n", current->data->pid, current->data->processorId, current->data->arrivalTime, current->data->finishTime, current->data->burstLength, current->data->remainingTime, timeQuantumRR, current->data->waitTime, current->data->turnaroundTime);
                        }
                        else {
                            printf("burst has finished: pid=%d, cpu=%d, arrivaltime=%ld, finishtime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d, waitingtime=%ld, turnaroundtime=%ld\n", current->data->pid, current->data->processorId, current->data->arrivalTime, current->data->finishTime, current->data->burstLength, current->data->remainingTime, timeQuantumRR, current->data->waitTime, current->data->turnaroundTime);
                        }
                    }

                    // Add the burst to the final process list using mutex lock

                    /* critical section begin */
                    pthread_mutex_lock(&(processList->mutex_lock));
                    if (processList->size == 0) {
                        processList->head = current;
                        processList->tail = current;
                    }
                    else {
                        processList->tail->next = current;
                        processList->tail = current;
                    }
                    (processList->size)++;
                    /* critical section end */
                    pthread_mutex_unlock(&(processList->mutex_lock)); 
                }

                else {
                    // Remove the process from the head of the queue
                    if (queueTable[queueID]->head == queueTable[queueID]->tail) {
                        queueTable[queueID]->tail = NULL;
                    }
                    queueTable[queueID]->head = queueTable[queueID]->head->next;
                    queueTable[queueID]->totalLoad -= timeQuantumRR;
                    (queueTable[queueID]->size)--;
                    current->next = NULL;

                    // Get timestamp before time quantum starts
                    gettimeofday(&tempTime, NULL);
                    tempTimeMS = ((tempTime.tv_sec) * 1000) + ((tempTime.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond

                    // Simulate burst with sleeping for time quantum
                    usleep(timeQuantumRR * 1000);

                    // Get timestamp after time quantum ends
                    gettimeofday(&tempTime2, NULL);
                    tempTimeMS2 = ((tempTime2.tv_sec) * 1000) + ((tempTime2.tv_usec) / 1000); // convert tv_sec & tv_usec to millisecond

                    // Insert the process to the tail of the queue
                    if (queueTable[queueID]->size == 0) {
                        queueTable[queueID]->head = current;
                        queueTable[queueID]->tail = current;
                    }
                    else {
                        queueTable[queueID]->tail->next = current;
                        queueTable[queueID]->tail = current;
                    }
                    (queueTable[queueID]->size)++;

                    // Update the task before releasing the lock since it is not done and can be used by another proces
                    oldRemainingTime = current->data->remainingTime;
                    current->data->remainingTime -= timeQuantumRR;
                    current->data->processorId = tid + 1;

                    /* critical section end */
                    pthread_mutex_unlock(&(queueTable[queueID]->mutex));

                    // Display the Info
                    if (outmode == 2) {
                        if (isOutFile == 1) {
                            fprintf(output_fp, "time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, oldRemainingTime);
                        }
                        else {
                            printf("time=%ld, cpu=%d, pid=%d, burstlen=%ld, remainingtime=%ld\n", (tempTimeMS - startTimeMS), (tid + 1), current->data->pid, current->data->burstLength, oldRemainingTime);
                        }
                    }

                    else if (outmode == 3) {
                        if (isOutFile == 1) {
                            fprintf(output_fp, "burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, oldRemainingTime, timeQuantumRR);
                        }
                        else {
                            printf("burst is picked for CPU: pid=%d, cpu=%d, arrivaltime=%ld, picktime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d\n", current->data->pid, (tid + 1), current->data->arrivalTime, (tempTimeMS - startTimeMS), current->data->burstLength, oldRemainingTime, timeQuantumRR);
                        }
                    }

                    
                    // Display the info for the burst that the time quantum expires  
                    if (outmode == 3) {
                        if (isOutFile == 1) {
                            fprintf(output_fp, "time quantum expired for burst (for Round Robin): pid=%d, cpu=%d, arrivaltime=%ld, expiretime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d\n", current->data->pid, current->data->processorId, current->data->arrivalTime, (tempTimeMS2 - startTimeMS), current->data->burstLength, current->data->remainingTime, timeQuantumRR);
                        }
                        else {
                            printf("time quantum expired for burst (for Round Robin): pid=%d, cpu=%d, arrivaltime=%ld, expiretime=%ld, burstlen=%ld, remainingtime=%ld, timequantum (for Raund Robin)=%d\n", current->data->pid, current->data->processorId, current->data->arrivalTime, (tempTimeMS2 - startTimeMS), current->data->burstLength, current->data->remainingTime, timeQuantumRR);
                        }
                    }
                }
            }
        }
    }

    if (outmode == 3) {
        if (isOutFile == 1) {
            fprintf(output_fp, "CPU with id=%d finished it's execution\n", (tid + 1));
        }
        else {
            printf("CPU with id=%d finished it's execution\n", (tid + 1));
        }
    }
    
    // Now that the dummy item is reached so thread can be terminated
    pthread_exit(NULL);
}