#include "campus.h"
#include <stdio.h>
#include <string.h>

static void usage(FILE *out)
{
    fputs("Usage:\n"
          "  ./campuscompute fcfs FILE\n"
          "  ./campuscompute rr FILE QUANTUM\n"
          "  ./campuscompute priority FILE\n"
          "  ./campuscompute compare FILE QUANTUM\n"
          "  ./campuscompute logger FILE\n"
          "  ./campuscompute banker FILE\n"
          "  ./campuscompute workers\n"
          "  ./campuscompute limits [N]\n"
          "  ./campuscompute ipc [ROUNDS]\n"
          "  ./campuscompute demo JOBS_FILE BANKER_FILE\n"
          "Each job line: ID arrival burst priority\n"
          "Banker file: PROCESSES RESOURCES, then Available, Allocation rows, "
          "Maximum rows ('#' starts a comment)\n"
          "limits: soft open-file limit N from 4 to 256 (default 8)\n"
          "ipc: additions per process from 1 to 100000 (default 50000)\n", out);
}

/* Person B commands. Returns the exit status, or -1 if cmd is not one of them. */
static int run_person_b(int argc, char **argv)
{
    const char *cmd = argv[1];
    int value;
    if (strcmp(cmd, "banker") == 0) {
        if (argc != 3) {
            fputs("Usage: ./campuscompute banker FILE\n", stderr);
            return 1;
        }
        return run_banker(argv[2]) == 1;      /* SAFE or UNSAFE both exit 0 */
    }
    if (strcmp(cmd, "workers") == 0) {
        if (argc != 2) {
            fputs("Usage: ./campuscompute workers   (no arguments)\n", stderr);
            return 1;
        }
        return run_workers();
    }
    if (strcmp(cmd, "limits") == 0) {
        value = 8;
        if (argc > 3 || (argc == 3 && (!read_number(argv[2], 4, &value) ||
                                       value > FD_LIMIT_MAX))) {
            fprintf(stderr, "Usage: ./campuscompute limits [N]   "
                    "(N is an integer from 4 to %d)\n", FD_LIMIT_MAX);
            return 1;
        }
        return run_limits(value);
    }
    if (strcmp(cmd, "ipc") == 0) {
        value = 50000;
        if (argc > 3 || (argc == 3 && !read_number(argv[2], 1, &value))) {
            fprintf(stderr, "Usage: ./campuscompute ipc [ROUNDS]   "
                    "(ROUNDS is an integer from 1 to %d)\n", MAX_VALUE);
            return 1;
        }
        return run_ipc(value);
    }
    if (strcmp(cmd, "demo") == 0) {
        if (argc != 4) {
            fputs("Usage: ./campuscompute demo JOBS_FILE BANKER_FILE\n", stderr);
            return 1;
        }
        return run_demo(argv[2], argv[3]);
    }
    return -1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }
    if (argc >= 2) {
        int result = run_person_b(argc, argv);
        if (result != -1)
            return result;
    }
    if (argc < 3) {
        usage(stderr);
        return 1;
    }
    const char *cmd = argv[1];
    int compare = strcmp(cmd, "compare") == 0;
    int logger = strcmp(cmd, "logger") == 0;
    Policy policy = FCFS;
    if (strcmp(cmd, "rr") == 0)
        policy = RR;
    else if (strcmp(cmd, "priority") == 0)
        policy = PRIORITY;
    else if (!compare && !logger && strcmp(cmd, "fcfs") != 0) {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(stderr);
        return 1;
    }

    int quantum = 0;
    int needs_quantum = (policy == RR || compare);
    if (argc != (needs_quantum ? 4 : 3)) {
        usage(stderr);
        return 1;
    }
    if (needs_quantum && !read_number(argv[3], 1, &quantum)) {
        fprintf(stderr, "Quantum must be an integer from 1 to %d.\n", MAX_VALUE);
        return 1;
    }
    Job jobs[MAX_JOBS];
    int count = read_jobs(argv[2], jobs);
    if (count < 0)
        return 1;
    if (logger)
        return run_logger(jobs, count);
    if (compare) {
        const char *name[] = {"FCFS", "RR", "Priority"};
        printf("%s: %d jobs, RR quantum %d\n", argv[2], count, quantum);
        puts("Policy       Avg CT   Avg TAT    Avg WT");
        for (int p = FCFS; p <= PRIORITY; p++) {
            Averages a = schedule(jobs, count, p, quantum, 0, NULL, NULL);
            printf("%-10s %8.2f %9.2f %9.2f\n", name[p], a.completion,
                   a.turnaround, a.waiting);
        }
    } else {
        schedule(jobs, count, policy, quantum, 1, NULL, NULL);
    }
    return 0;
}
