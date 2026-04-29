#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#define QUEUESIZE 10
#define PROD_LOOP 1000 // Συγκεκριμένος μεγάλος αριθμός επαναλήψεων για producers
#define P 10 // Αριθμός Producers
#define Q 1 // Αριθμός Consumers

// 1. Ορισμός της δομής workFunction
typedef struct {
    void *(*work)(void *);
    void *arg;
    struct timeval timestamp; // Χρονοσφραγίδα εισαγωγής
} workFunction;

typedef struct {
    workFunction buf[QUEUESIZE]; // Η ουρά κρατάει πλέον workFunction
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

// Πρωτότυπα συναρτήσεων ουράς
queue *queueInit(void);
void queueDelete(queue *q);
void queueAdd(queue *q, workFunction in);
void queueDel(queue *q, workFunction *out);

// Η "απλή" συνάρτηση που θα εκτελούν οι consumers
void *calculate_sine(void *arg) {
    double angle = *(double *)arg;
    double result = sin(angle);
    printf("Result: sin(%.2f) = %.4f\n", angle, result);
    free(arg); // Απελευθέρωση μνήμης που δέσμευσε ο producer
    return NULL;
}

// 2. Producer: Βάζει δείκτες εργασιών στην ουρά
void *producer(void *q) {
    queue *fifo = (queue *)q;

    for (int i = 0; i < PROD_LOOP; i++) {
        // Προετοιμασία της εργασίας
        workFunction wf;
        wf.work = calculate_sine;
        double *angle = malloc(sizeof(double));
        *angle = (double)i * 0.1; 
        wf.arg = angle;

        pthread_mutex_lock(fifo->mut);
        while (fifo->full) {
            pthread_cond_wait(fifo->notFull, fifo->mut);
        }
        gettimeofday(&wf.timestamp, NULL);
        queueAdd(fifo, wf);
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
        // Χωρίς sleep() όπως ζητήθηκε
    }
    return NULL;
}

// 2 & 3. Consumer: Παίρνει εργασίες και τις εκτελεί (while 1)
void *consumer(void *q) {
    queue *fifo = (queue *)q;
    workFunction work_to_do;

    while (1) { // 3. consumer while 1
        pthread_mutex_lock(fifo->mut);
        while (fifo->empty) {
            pthread_cond_wait(fifo->notEmpty, fifo->mut);
        }
        queueDel(fifo, &work_to_do);
        struct timeval pop_time;
        gettimeofday(&pop_time, NULL);
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notFull);

        struct timeval end_time;
        gettimeofday(&end_time, NULL); // Σφραγίδα χρόνου παραλαβής

        long seconds = end_time.tv_sec - work_to_do.timestamp.tv_sec;
        long microseconds = end_time.tv_usec - work_to_do.timestamp.tv_usec;
        long total_latency = (seconds * 1000000) + microseconds;

printf("Waiting time in queue: %ld us\n", total_latency);

        // Εκτέλεση της συνάρτησης
        work_to_do.work(work_to_do.arg);
    }
    return NULL;
}

int main() {
    queue *fifo;
    pthread_t pro[P], con[Q];

    fifo = queueInit();
    if (fifo == NULL) {
        exit(1);
    }

    // Δημιουργία P producers
    for (int i = 0; i < P; i++) 
        pthread_create(&pro[i], NULL, producer, fifo);
    
    // Δημιουργία Q consumers
    for (int i = 0; i < Q; i++) 
        pthread_create(&con[i], NULL, consumer, fifo);

    // Join τους producers
    for (int i = 0; i < P; i++) 
        pthread_join(pro[i], NULL);

    // Σημείωση: Οι consumers τρέχουν με while(1), σε πραγματικό σενάριο 
    // θα χρειαζόμασταν ένα "kill pill" (ειδικό flag) για να σταματήσουν τώρα.
    
    printf("Producers finished. Main thread exiting.\n");
    return 0;
}

// --- Υλοποίηση Ουράς (Προσαρμοσμένη για workFunction) ---

queue *queueInit(void) {
    queue *q = (queue *)malloc(sizeof(queue));
    q->empty = 1; q->full = 0; q->head = 0; q->tail = 0;
    q->mut = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->mut, NULL);
    q->notFull = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notFull, NULL);
    q->notEmpty = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->notEmpty, NULL);
    return q;
}

void queueAdd(queue *q, workFunction in) {
    q->buf[q->tail] = in;
    q->tail = (q->tail + 1) % QUEUESIZE;
    if (q->tail == q->head) q->full = 1;
    q->empty = 0;
}

void queueDel(queue *q, workFunction *out) {
    *out = q->buf[q->head];
    q->head = (q->head + 1) % QUEUESIZE;
    if (q->head == q->tail) q->empty = 1;
    q->full = 0;
}