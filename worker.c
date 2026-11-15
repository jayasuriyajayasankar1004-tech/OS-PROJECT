/* FR6: Worker failure detection.
 * The parent fork()s three child workers. One exits normally; the other two
 * fail ON PURPOSE (clearly labelled SIMULATED). The parent uses waitpid() and
 * decodes each status with WIFEXITED / WEXITSTATUS / WIFSIGNALED / WTERMSIG. */
#define _POSIX_C_SOURCE 200809L
#include "campus.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define WORKER_COUNT 3
#define SIMULATED_EXIT_CODE 42

typedef enum { WORK_NORMAL, WORK_SIMULATED_EXIT, WORK_SIMULATED_CRASH } Behaviour;

static const char *plan[WORKER_COUNT] = {
    "normal: add 1..1000, then exit(0)",
    "SIMULATED failure: exit with code 42",
    "SIMULATED crash: send itself SIGKILL",
};

/* Runs only inside the child. Never returns: always ends with _exit(). */
static void worker_main(int number, Behaviour behaviour)
{
    long pid = (long)getpid();
    if (behaviour == WORK_NORMAL) {
        long sum = 0;
        for (int i = 1; i <= 1000; i++)
            sum += i;
        printf("  [worker %d, pid %ld] sum 1..1000 = %ld, exiting with code 0\n",
               number, pid, sum);
        fflush(stdout);
        _exit(0);
    }
    if (behaviour == WORK_SIMULATED_EXIT) {
        printf("  [worker %d, pid %ld] SIMULATED failure: exiting with code %d\n",
               number, pid, SIMULATED_EXIT_CODE);
        fflush(stdout);
        _exit(SIMULATED_EXIT_CODE);
    }
    printf("  [worker %d, pid %ld] SIMULATED crash: raising SIGKILL on itself\n",
           number, pid);
    fflush(stdout);
    raise(SIGKILL);
    _exit(1);                   /* not reached: SIGKILL cannot be caught */
}

int run_workers(void)
{
    pid_t pids[WORKER_COUNT];
    int started = 0, failed = 0, status_code = 0;

    printf("Parent pid %ld starting %d workers (failures below are SIMULATED)\n",
           (long)getpid(), WORKER_COUNT);
    for (int i = 0; i < WORKER_COUNT; i++) {
        /* Flush first, or the child gets a copy of unprinted text and prints it twice. */
        fflush(stdout);
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            status_code = 1;
            break;              /* still wait for the workers already started */
        }
        if (pid == 0)
            worker_main(i + 1, (Behaviour)i);
        pids[started++] = pid;
        printf("Started worker %d as pid %ld (%s)\n", i + 1, (long)pid, plan[i]);
    }

    puts("Parent waiting with waitpid() and decoding each status:");
    for (int i = 0; i < started; i++) {
        int status;
        pid_t result;
        do {
            result = waitpid(pids[i], &status, 0);
        } while (result == -1 && errno == EINTR);   /* retry if a signal interrupted us */
        if (result == -1) {
            perror("waitpid");
            status_code = 1;
            continue;
        }
        const char *tag = (i == WORK_NORMAL) ? "" : " [SIMULATED]";
        printf("  worker %d (pid %ld): ", i + 1, (long)result);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            puts("WIFEXITED, exit code 0 -> OK");
        } else if (WIFEXITED(status)) {
            printf("WIFEXITED, exit code %d -> FAILURE DETECTED%s\n",
                   WEXITSTATUS(status), tag);
            failed++;
        } else if (WIFSIGNALED(status)) {
            printf("WIFSIGNALED, signal %d (%s) -> FAILURE DETECTED%s\n",
                   WTERMSIG(status), strsignal(WTERMSIG(status)), tag);
            failed++;
        } else {
            printf("unexpected status 0x%x\n", (unsigned)status);
            failed++;
        }
    }
    printf("Summary: %d workers, %d ok, %d failed (all failures were SIMULATED). "
           "All children reaped.\n", started, started - failed, failed);
    return status_code;
}
