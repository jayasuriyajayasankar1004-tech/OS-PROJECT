/* Integrated demo: runs Person A's scheduler and logger and Person B's
 * Banker, workers, limits and IPC modules one after another, then prints a
 * summary built from what each step actually returned. */
#define _POSIX_C_SOURCE 200809L
#include "campus.h"
#include <stdio.h>

#define DEMO_QUANTUM 2
#define DEMO_FD_LIMIT 8
#define DEMO_IPC_ROUNDS 20000

static void heading(int step, const char *title)
{
    printf("\n===== [%d] %s =====\n", step, title);
    fflush(stdout);
}

static const char *ok_text(int failed)
{
    return failed ? "FAILED" : "ok";
}

int run_demo(const char *jobs_path, const char *banker_path)
{
    Job jobs[MAX_JOBS];
    int count = read_jobs(jobs_path, jobs);
    if (count < 0)
        return 1;

    heading(1, "FCFS scheduling (Person A)");
    Averages fcfs = schedule(jobs, count, FCFS, 0, 1, NULL, NULL);
    heading(2, "Round Robin scheduling, quantum 2 (Person A)");
    Averages rr = schedule(jobs, count, RR, DEMO_QUANTUM, 1, NULL, NULL);
    heading(3, "Priority scheduling (Person A)");
    Averages prio = schedule(jobs, count, PRIORITY, 0, 1, NULL, NULL);
    heading(4, "Producer-consumer logger (Person A)");
    int logger = run_logger(jobs, count);
    heading(5, "Banker's safety algorithm (Person B)");
    int banker = run_banker(banker_path);
    heading(6, "Worker failure detection (Person B)");
    int workers = run_workers();
    heading(7, "Resource limit RLIMIT_NOFILE (Person B)");
    int limits = run_limits(DEMO_FD_LIMIT);
    heading(8, "Shared memory + named semaphore (Person B)");
    int ipc = run_ipc(DEMO_IPC_ROUNDS);

    int failures = (logger != 0) + (banker == 1) + (workers != 0) +
                   (limits != 0) + (ipc != 0);
    const char *banker_result = banker == 0 ? "SAFE" : banker == 2 ? "UNSAFE" : "input error";

    puts("\n===== Summary =====");
    printf("%-26s ok      avg WT %.2f\n", "1 FCFS", fcfs.waiting);
    printf("%-26s ok      avg WT %.2f\n", "2 Round Robin (q=2)", rr.waiting);
    printf("%-26s ok      avg WT %.2f\n", "3 Priority", prio.waiting);
    printf("%-26s %-7s activity.log written\n", "4 Logger", ok_text(logger));
    printf("%-26s %-7s %s (%s)\n", "5 Banker", ok_text(banker == 1), banker_result,
           banker_path);
    printf("%-26s %-7s failures shown were SIMULATED\n", "6 Workers", ok_text(workers));
    printf("%-26s %-7s soft limit %d hit, then restored\n", "7 Limits", ok_text(limits),
           DEMO_FD_LIMIT);
    printf("%-26s %-7s %d x 2 additions, objects removed\n", "8 IPC", ok_text(ipc),
           DEMO_IPC_ROUNDS);
    printf("Overall: %d of 8 steps completed without error\n", 8 - failures);
    return failures ? 1 : 0;
}
