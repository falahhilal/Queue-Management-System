#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_PEOPLE 40
#define NUM_COUNTERS 3

typedef enum { ID_CARD, BIRTH_CERT, DEATH_CERT } ServiceType;
const char* serviceNames[] = { "ID Card", "Birth Certificate", "Death Certificate" };

typedef struct {      //person
    int id;
    int ticketNo;
    ServiceType service;
    pthread_cond_t cond;
    int isServed;
    int hasBirthCertificate;   // flag to track if person already has birth certificate
    int hasIDCard;             // flag to track if person already has ID card
} Person;

typedef struct Node {        //for queue
    Person* person;
    struct Node* next;
} Node;

//1 queue for each counter
Node* queues[NUM_COUNTERS] = { NULL };
pthread_mutex_t queueMutex[NUM_COUNTERS];
pthread_cond_t queueCond[NUM_COUNTERS];    
int ticketCounter = 0;

void enqueue(Node** head, Person* p) {          //adds person to a queue
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

Person* front(Node* head) {                    //to get the next person in queue
    return head ? head->person : NULL;
}

void dequeue(Node** head) {                   //when service is done remove them from queue
    if (*head == NULL) return;
    Node* temp = *head;
    *head = (*head)->next;
    free(temp);
}

void requestService(Person* p, ServiceType serviceType) {
    //give ticket and join queue
    pthread_mutex_lock(&queueMutex[serviceType]);
    p->ticketNo = ticketCounter++;
    p->isServed = 0;
    pthread_cond_init(&p->cond, NULL);
    enqueue(&queues[serviceType], p);
    printf("Person #%d requested %s with ticket #%d\n",
           p->id, serviceNames[serviceType], p->ticketNo);

    //wait
    while (front(queues[serviceType]) != p) {
        pthread_cond_wait(&p->cond, &queueMutex[serviceType]);           //wait condition until person is at front
    }

    //at counter
    printf("Person #%d is being served at %s counter\n",
           p->id, serviceNames[serviceType]);
    pthread_mutex_unlock(&queueMutex[serviceType]);

    sleep(1); //service time

    pthread_mutex_lock(&queueMutex[serviceType]);
    printf("Person #%d done at %s counter\n",
           p->id, serviceNames[serviceType]);
    dequeue(&queues[serviceType]);
    //notify next person in the queue
    if (queues[serviceType]) {
        pthread_cond_signal(&queues[serviceType]->person->cond);       //informing the person in front
    }
    pthread_mutex_unlock(&queueMutex[serviceType]);

    //set flags
    if (serviceType == BIRTH_CERT) p->hasBirthCertificate = 1;
    if (serviceType == ID_CARD)    p->hasIDCard = 1;
}

void* personThread(void* arg) {
    Person* p = (Person*)arg;

    //check conditions
    if (p->service == ID_CARD && !p->hasBirthCertificate) {
        printf("Person #%d wants an ID Card but doesn't have a Birth Certificate.\n", p->id);
        printf("Redirecting Person #%d to Birth Certificate counter first.\n", p->id);
        requestService(p, BIRTH_CERT);
    }

    if (p->service == DEATH_CERT) {
        if (!p->hasBirthCertificate) {
            printf("Person #%d wants a Death Certificate but doesn't have a Birth Certificate.\n", p->id);
            printf("Redirecting Person #%d to Birth Certificate counter first.\n", p->id);
            requestService(p, BIRTH_CERT);
        }
        if (!p->hasIDCard) {
            printf("Person #%d wants a Death Certificate but doesn't have an ID Card.\n", p->id);
            printf("Redirecting Person #%d to ID Card counter first.\n", p->id);
            // ID Card requires birth cert too, but by now it's already ensured
            requestService(p, ID_CARD);
        }
    }

    //finally request the original service
    requestService(p, p->service);

    pthread_exit(NULL);
}

int main() {
    pthread_t people[NUM_PEOPLE];
    srand(time(NULL));

    for (int i = 0; i < NUM_COUNTERS; i++) {           //initialization
        pthread_mutex_init(&queueMutex[i], NULL);
        pthread_cond_init(&queueCond[i], NULL);
    }

    for (int i = 0; i < NUM_PEOPLE; i++) {          
        Person* p = malloc(sizeof(Person));
        p->id = i;
        p->service = rand() % NUM_COUNTERS;
        p->hasBirthCertificate = rand() % 2;  //randomly assign if they already have a birth certificate
        p->hasIDCard = rand() % 2;           //randomly assign if they already have an ID card
        pthread_create(&people[i], NULL, personThread, p);
        sleep(1);
    }

    for (int i = 0; i < NUM_PEOPLE; i++) {   //join all threads
        pthread_join(people[i], NULL);
    }

    return 0;
}