#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

#define NUM_PEOPLE 40
#define NUM_COUNTERS 3

typedef enum { ID_CARD, BIRTH_CERT, DEATH_CERT } ServiceType;
const char* serviceNames[] = { "ID Card", "Birth Certificate", "Death Certificate" };

typedef struct {
    int id;
    int ticketNo;
    ServiceType service;
    sem_t sem;             
    int isServed;
    int hasBirthCertificate;
    int hasIDCard;
} Person;

typedef struct Node {
    Person* person;
    struct Node* next;
} Node;

Node* queues[NUM_COUNTERS] = { NULL };
pthread_mutex_t queueMutex[NUM_COUNTERS];
pthread_mutex_t ticketMutex;
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

void* counterThread(void* arg) {
    ServiceType type = *(ServiceType*)arg;
    while (1) {
        pthread_mutex_lock(&queueMutex[type]);

        if (queues[type] != NULL) {
            Person* p = front(queues[type]);
            printf("Person #%d is being served at %s counter\n", p->id, serviceNames[type]);
            sem_post(&p->sem);  // Wake up person
            pthread_mutex_unlock(&queueMutex[type]);

            int serviceTime = rand() % 3 + 1;  // 1 to 3 seconds
            //int serviceTime = 2;
            sleep(serviceTime);

            pthread_mutex_lock(&queueMutex[type]);
            printf("Person #%d done at %s counter (Service Time: %ds)\n", p->id, serviceNames[type], serviceTime);
            dequeue(&queues[type]);
            pthread_mutex_unlock(&queueMutex[type]);

            // set flags
            if (type == BIRTH_CERT) p->hasBirthCertificate = 1;
            if (type == ID_CARD)    p->hasIDCard = 1;

            sem_destroy(&p->sem);
        } else {
            pthread_mutex_unlock(&queueMutex[type]);
            usleep(100000); // avoid busy waiting (100ms)
        }
    }
    return NULL;
}

void requestService(Person* p, ServiceType serviceType) {
    pthread_mutex_lock(&queueMutex[serviceType]);

    pthread_mutex_lock(&ticketMutex);
    p->ticketNo = ticketCounter++;
    pthread_mutex_unlock(&ticketMutex);

    p->isServed = 0;

    sem_init(&p->sem, 0, 0); 
    enqueue(&queues[serviceType], p);
    printf("Person #%d requested %s with ticket #%d\n",
           p->id, serviceNames[serviceType], p->ticketNo);

    pthread_mutex_unlock(&queueMutex[serviceType]);

    sem_wait(&p->sem); // wait to be served
}

void* personThread(void* arg) {
    Person* p = (Person*)arg;

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
            requestService(p, ID_CARD);
        }
    }

    requestService(p, p->service);
    pthread_exit(NULL);
}

int main() {
    pthread_t people[NUM_PEOPLE];
    pthread_t counters[NUM_COUNTERS];
    srand(time(NULL));
    pthread_mutex_init(&ticketMutex, NULL);

    for (int i = 0; i < NUM_COUNTERS; i++) {
        pthread_mutex_init(&queueMutex[i], NULL);
        ServiceType* arg = malloc(sizeof(ServiceType));
        *arg = i;
        pthread_create(&counters[i], NULL, counterThread, arg);
    }

    for (int i = 0; i < NUM_PEOPLE; i++) {
        Person* p = malloc(sizeof(Person));
        p->id = i;
        p->service = rand() % NUM_COUNTERS;
        p->hasBirthCertificate = rand() % 2;
        p->hasIDCard = rand() % 2;
        pthread_create(&people[i], NULL, personThread, p);
        sleep(1); // space out arrival times
    }

    for (int i = 0; i < NUM_PEOPLE; i++) {
        pthread_join(people[i], NULL);
    }

    // In production, you would use a proper signal to shut down counter threads
    printf("\nAll people have been served.\n");
    return 0;
}

