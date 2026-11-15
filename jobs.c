#include "campus.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_number(const char *text, int minimum, int *result)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < minimum || value > MAX_VALUE)
        return 0;
    *result = (int)value;
    return 1;
}

static int parse_job(char *line, Job *job)
{
    const char *space = " \t\r\n";
    char *field = strtok(line, space);
    if (!field)
        return 0;
    int *values[] = {&job->id, &job->arrival, &job->burst, &job->priority};
    int minimum[] = {1, 0, 1, 0};
    for (int i = 0; i < 4; i++) {
        if (!field || !read_number(field, minimum[i], values[i]))
            return -1;
        field = strtok(NULL, space);
    }
    return field ? -1 : 1;
}

int read_jobs(const char *path, Job jobs[])
{
    FILE *file = fopen(path, "r");
    if (!file) {
        perror(path);
        return -1;
    }
    int count = 0, line_number = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        Job job = {0};
        line_number++;
        if (!strchr(line, '\n') && !feof(file)) {
            fprintf(stderr, "%s:%d: line too long\n", path, line_number);
            goto error;
        }
        int result = parse_job(line, &job);
        if (result == 0)
            continue;
        if (result < 0) {
            fprintf(stderr, "%s:%d: expected ID arrival burst priority "
                    "(ID and burst > 0, others >= 0, maximum %d)\n",
                    path, line_number, MAX_VALUE);
            goto error;
        }
        if (count == MAX_JOBS) {
            fprintf(stderr, "Maximum %d jobs per file.\n", MAX_JOBS);
            goto error;
        }
        for (int i = 0; i < count; i++) {
            if (jobs[i].id == job.id) {
                fprintf(stderr, "%s:%d: duplicate job ID %d\n", path, line_number, job.id);
                goto error;
            }
        }
        job.remaining = job.burst;
        job.status = NEW;
        jobs[count++] = job;
    }
    if (ferror(file)) {
        perror(path);
        goto error;
    }
    if (!count) {
        fprintf(stderr, "%s: job file is empty\n", path);
        goto error;
    }
    fclose(file);
    return count;

error:
    fclose(file);
    return -1;
}
