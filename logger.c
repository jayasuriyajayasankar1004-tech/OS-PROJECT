#include "campus.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define CAPACITY 4
#define MESSAGE_SIZE 80

typedef struct {
    char items[CAPACITY][MESSAGE_SIZE];
    int head, tail, size, peak, produced, written, finished;
    pthread_mutex_t lock;
    pthread_cond_t not_empty, not_full;
    const Job *jobs;
    int count;
    FILE *file;
} Logger;

static void enqueue(const char *message, void *context)
{
    Logger *log = context;
    pthread_mutex_lock(&log->lock);
    while (log->size == CAPACITY)
        pthread_cond_wait(&log->not_full, &log->lock);
    snprintf(log->items[log->tail], MESSAGE_SIZE, "%s", message);
    log->tail = (log->tail + 1) % CAPACITY;
    log->size++;
    if (log->size > log->peak)
        log->peak = log->size;
    log->produced++;
    pthread_cond_signal(&log->not_empty);
    pthread_mutex_unlock(&log->lock);
}

static void *produce(void *arg)
{
    Logger *log = arg;
    schedule(log->jobs, log->count, FCFS, 0, 0, enqueue, log);
    return NULL;
}

static void *consume(void *arg)
{
    Logger *log = arg;
    char message[MESSAGE_SIZE];
    for (;;) {
        pthread_mutex_lock(&log->lock);
        while (!log->size && !log->finished)
            pthread_cond_wait(&log->not_empty, &log->lock);
        /* After the producer exits, write any messages still in the queue. */
        if (!log->size && log->finished) {
            pthread_mutex_unlock(&log->lock);
            break;
        }
        strcpy(message, log->items[log->head]);
        log->head = (log->head + 1) % CAPACITY;
        log->size--;
        pthread_cond_signal(&log->not_full);
        pthread_mutex_unlock(&log->lock);

        if (fprintf(log->file, "%s\n", message) >= 0)
            log->written++;
    }
    return NULL;
}

int run_logger(const Job jobs[], int count)
{
    Logger log = {
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .not_empty = PTHREAD_COND_INITIALIZER, .not_full = PTHREAD_COND_INITIALIZER,
        .jobs = jobs, .count = count
    };
    pthread_t producer, consumer;
    int status = 1;
    log.file = fopen("activity.log", "w");
    if (!log.file) {
        perror("activity.log");
        goto cleanup;
    }
    int error = pthread_create(&consumer, NULL, consume, &log);
    if (error) {
        fprintf(stderr, "Cannot start consumer: %s\n", strerror(error));
        goto close_file;
    }
    error = pthread_create(&producer, NULL, produce, &log);
    if (error)
        fprintf(stderr, "Cannot start producer: %s\n", strerror(error));
    else
        pthread_join(producer, NULL);

    pthread_mutex_lock(&log.lock);
    log.finished = 1;
    pthread_cond_signal(&log.not_empty);
    pthread_mutex_unlock(&log.lock);
    pthread_join(consumer, NULL);
    if (!error && !ferror(log.file)) {
        printf("Producer queued %d FCFS events. Consumer wrote %d events.\n",
               log.produced, log.written);
        printf("Queue peak: %d/%d slots. Remaining: %d.\n", log.peak, CAPACITY, log.size);
        puts("Producer and consumer joined.");
        status = 0;
    } else if (ferror(log.file)) {
        fputs("Failed to write activity.log\n", stderr);
    }
close_file:
    if (fclose(log.file) == EOF) {
        perror("activity.log");
        status = 1;
    }
    if (status == 0)
        puts("Log saved to activity.log");
cleanup:
    pthread_cond_destroy(&log.not_empty);
    pthread_cond_destroy(&log.not_full);
    pthread_mutex_destroy(&log.lock);
    return status;
}
