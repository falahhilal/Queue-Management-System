#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_PEOPLE 10
#define NUM_COUNTERS 3

typedef enum { ID_CARD, BIRTH_CERT, DEATH_CERT } ServiceType;
const char* serviceNames[] = { "ID Card", "Birth Certificate", "Death Certificate" };

typedef struct {
    int ticketNo;
    ServiceType service;
    pthread_cond_t cond;
    int isServed;
} Person;

typedef struct Node {
    Person* person;
    struct Node* next;
} Node;

//queue for each counter
Node* queues[NUM_COUNTERS] = { NULL };
pthread_mutex_t queueMutex[NUM_COUNTERS];
pthread_cond_t queueCond[NUM_COUNTERS];
int ticketCounter = 0;

void enqueue(Node** head, Person* p) {
    Node* newNode = malloc(sizeof(Node));
    newNode->person = p;
    newNode->next = NULL;

    if (*head == NULL) {
        *head = newNode;
        return;
    }
    Node* temp = *head;
    while (temp->next) temp = temp->next;
    temp->next = newNode;
}

Person* front(Node* head) {
    return head ? head->person : NULL;
}

void dequeue(Node** head) {
    if (*head == NULL) return;
    Node* temp = *head;
    *head = (*head)->next;
    free(temp);
}

void* personThread(void* arg) {
    Person* p = (Person*)arg;

    //give ticket and join queue
    pthread_mutex_lock(&queueMutex[p->service]);
    p->ticketNo = ticketCounter++;
    p->isServed = 0;
    pthread_cond_init(&p->cond, NULL);
    enqueue(&queues[p->service], p);
    printf("Person %d requested %s with ticket #%d\n", 
           (int)pthread_self(), serviceNames[p->service], p->ticketNo);

    //wait
    while (front(queues[p->service]) != p) {
        pthread_cond_wait(&p->cond, &queueMutex[p->service]);
    }

    //at counter
    printf("Person #%d is being served at %s counter\n", 
           p->ticketNo, serviceNames[p->service]);
    pthread_mutex_unlock(&queueMutex[p->service]);

    sleep(1); //service time

    pthread_mutex_lock(&queueMutex[p->service]);
    printf("Person #%d done at %s counter\n", p->ticketNo, serviceNames[p->service]);
    dequeue(&queues[p->service]);
    //notify next person in the queue
    if (queues[p->service]) {
        pthread_cond_signal(&queues[p->service]->person->cond);
    }
    pthread_mutex_unlock(&queueMutex[p->service]);

    pthread_exit(NULL);
}

int main() {
    pthread_t people[NUM_PEOPLE];
    srand(time(NULL));

    for (int i = 0; i < NUM_COUNTERS; i++) {
        pthread_mutex_init(&queueMutex[i], NULL);
        pthread_cond_init(&queueCond[i], NULL);
    }

    for (int i = 0; i < NUM_PEOPLE; i++) {
        Person* p = malloc(sizeof(Person));
        p->service = rand() % NUM_COUNTERS;
        pthread_create(&people[i], NULL, personThread, p);
        sleep(1);
    }

    for (int i = 0; i < NUM_PEOPLE; i++) {
        pthread_join(people[i], NULL);
    }

    return 0;
}

   
