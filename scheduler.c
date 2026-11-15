#include "campus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int arrival_order(const void *left, const void *right)
{
    const Job *a = left, *b = right;
    if (a->arrival != b->arrival)
        return a->arrival - b->arrival;
    return a->id - b->id;
}

static void event(EventWriter writer, void *context, int time, int id, const char *action)
{
    if (writer) {
        char message[80];
        snprintf(message, sizeof(message), "t=%d  J%d  %s", time, id, action);
        writer(message, context);
    }
}

static void admit(Job jobs[], int count, int time, int *next, int ready[],
                  int *size, EventWriter writer, void *context)
{
    while (*next < count && jobs[*next].arrival <= time) {
        Job *job = &jobs[*next];
        job->status = READY;
        ready[(*size)++] = (*next)++;
        event(writer, context, job->arrival, job->id, "ready");
    }
}

Averages schedule(const Job input[], int count, Policy policy, int quantum,
                  int show, EventWriter writer, void *context)
{
    Job jobs[MAX_JOBS];
    int ready[MAX_JOBS];
    int next = 0, size = 0, time = 0;
    Averages averages = {0};
    memcpy(jobs, input, count * sizeof(Job));
    qsort(jobs, count, sizeof(Job), arrival_order);

    if (show) {
        if (policy == RR)
            printf("Round Robin (quantum %d). Arrivals join before requeue.\n", quantum);
        else if (policy == PRIORITY)
            puts("Priority (non-preemptive). Smaller priority number runs first.");
        else
            puts("FCFS (non-preemptive).");
        puts("Ties: earlier arrival, then smaller job ID.");
        puts("Timeline:");
    }
    while (next < count || size > 0) {
        if (size == 0 && time < jobs[next].arrival) {
            if (show)
                printf("  %d-%d idle\n", time, jobs[next].arrival);
            time = jobs[next].arrival;
        }
        admit(jobs, count, time, &next, ready, &size, writer, context);

        int pick = 0;
        if (policy == PRIORITY) {
            for (int i = 1; i < size; i++)
                if (jobs[ready[i]].priority < jobs[ready[pick]].priority)
                    pick = i;
        }
        int index = ready[pick];
        for (int i = pick + 1; i < size; i++)
            ready[i - 1] = ready[i];
        size--;
        Job *job = &jobs[index];
        job->status = RUNNING;
        event(writer, context, time, job->id, "running");
        int slice = job->remaining;
        if (policy == RR && slice > quantum)
            slice = quantum;
        if (show)
            printf("  %d-%d J%d\n", time, time + slice, job->id);
        time += slice;
        job->remaining -= slice;

        /* Jobs arriving at the slice boundary get in line before the old job. */
        admit(jobs, count, time, &next, ready, &size, writer, context);
        if (job->remaining == 0) {
            job->completion = time;
            job->status = DONE;
            event(writer, context, time, job->id, "completed");
        } else {
            job->status = READY;
            ready[size++] = index;
            event(writer, context, time, job->id, "requeued");
        }
    }

    if (show)
        printf("%-8s %7s %6s %8s %7s %7s %7s  %s\n",
               "Job", "Arrival", "Burst", "Priority", "CT", "TAT", "WT", "Status");
    for (int i = 0; i < count; i++) {
        Job *job = &jobs[i];
        int turnaround = job->completion - job->arrival;
        int waiting = turnaround - job->burst;
        averages.completion += job->completion;
        averages.turnaround += turnaround;
        averages.waiting += waiting;
        if (show)
            printf("J%-7d %7d %6d %8d %7d %7d %7d  %s\n", job->id,
                   job->arrival, job->burst, job->priority, job->completion,
                   turnaround, waiting, job->status == DONE ? "done" : "pending");
    }
    averages.completion /= count;
    averages.turnaround /= count;
    averages.waiting /= count;
    if (show)
        printf("Averages: CT %.2f   TAT %.2f   WT %.2f\n",
               averages.completion, averages.turnaround, averages.waiting);
    return averages;
}
