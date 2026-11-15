#ifndef CAMPUS_H
#define CAMPUS_H

#define MAX_JOBS 64
#define MAX_VALUE 100000

typedef enum { NEW, READY, RUNNING, DONE } Status;
typedef enum { FCFS, RR, PRIORITY } Policy;

typedef struct {
    int id, arrival, burst, priority;
    int remaining, completion;
    Status status;
} Job;

typedef struct {
    double completion, turnaround, waiting;
} Averages;

typedef void (*EventWriter)(const char *message, void *context);

int read_jobs(const char *path, Job jobs[]);
int read_number(const char *text, int minimum, int *result);
Averages schedule(const Job input[], int count, Policy policy, int quantum,
                  int show, EventWriter write_event, void *context);
int run_logger(const Job jobs[], int count);

/* Person B */
#define FD_LIMIT_MAX 256          /* largest soft limit the limits demo accepts */
int run_banker(const char *path); /* 0 = SAFE, 2 = UNSAFE, 1 = bad input */
int run_workers(void);            /* 0 = all workers reaped, 1 = error */
int run_limits(int new_soft);     /* 0 = limit hit and restored, 1 = error */
int run_ipc(int rounds);          /* 0 = counter correct and cleaned up, 1 = error */
int run_demo(const char *jobs_path, const char *banker_path);

#endif
