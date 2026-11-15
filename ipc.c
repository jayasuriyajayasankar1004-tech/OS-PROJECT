/* FR4: POSIX shared memory + named semaphore.
 * Parent and child both add 1 to a counter that lives in shared memory,
 * ROUNDS times each. A named semaphore (value 1) acts as a lock, so no update
 * is lost and the final counter must equal 2 * ROUNDS. Every call is checked,
 * and only the two objects this run created are removed afterwards. */
#define _POSIX_C_SOURCE 200809L
#include "campus.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    long counter;          /* the value both processes update */
    long parent_adds;
    long child_adds;
    char message[80];      /* child leaves a note for the parent */
} SharedData;

/* Adds 1 to the shared counter 'rounds' times, holding the semaphore each time. */
static int add_rounds(SharedData *shared, sem_t *lock, int rounds, long *my_adds)
{
    for (int i = 0; i < rounds; i++) {
        while (sem_wait(lock) == -1) {        /* lock: value 1 -> 0, or wait */
            if (errno != EINTR) {
                perror("sem_wait");
                return -1;
            }
        }
        shared->counter++;                    /* critical section */
        (*my_adds)++;
        if (sem_post(lock) == -1) {           /* unlock: value 0 -> 1 */
            perror("sem_post");
            return -1;
        }
    }
    return 0;
}

int run_ipc(int rounds)
{
    char shm_name[64], sem_name[64], path[96];
    int fd = -1, status = 1, shm_created = 0, sem_created = 0;
    SharedData *shared = MAP_FAILED;
    sem_t *lock = SEM_FAILED;

    /* Put our pid in the names so we never touch another program's objects. */
    snprintf(shm_name, sizeof(shm_name), "/campuscompute_shm_%ld", (long)getpid());
    snprintf(sem_name, sizeof(sem_name), "/campuscompute_sem_%ld", (long)getpid());

    fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) {
        perror("shm_open");
        goto cleanup;
    }
    shm_created = 1;
    printf("shm_open:  created %s (visible as /dev/shm%s)\n", shm_name, shm_name);

    if (ftruncate(fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        goto cleanup;
    }
    printf("ftruncate: size set to %zu bytes\n", sizeof(SharedData));

    shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }
    puts("mmap:      mapped with MAP_SHARED (fork keeps the same mapping)");
    /* The mapping stays valid after the descriptor is closed. */
    if (close(fd) == -1) {
        perror("close");
        fd = -1;
        goto cleanup;
    }
    fd = -1;
    memset(shared, 0, sizeof(SharedData));

    lock = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 1);
    if (lock == SEM_FAILED) {
        perror("sem_open");
        goto cleanup;
    }
    sem_created = 1;
    printf("sem_open:  created %s with value 1 (visible as /dev/shm/sem.%s)\n",
           sem_name, sem_name + 1);

    printf("Parent and child will each add 1 to the shared counter %d times.\n", rounds);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        goto cleanup;
    }
    if (pid == 0) {
        /* Child: work, leave a message, close its own handles, exit.
         * Only the parent unlinks, because it created the objects. */
        int child_ok = add_rounds(shared, lock, rounds, &shared->child_adds) == 0;
        if (child_ok && sem_wait(lock) == 0) {
            snprintf(shared->message, sizeof(shared->message),
                     "hello from child pid %ld", (long)getpid());
            if (sem_post(lock) == -1)
                child_ok = 0;
        }
        if (munmap(shared, sizeof(SharedData)) == -1) {
            perror("child munmap");
            child_ok = 0;
        }
        if (sem_close(lock) == -1) {
            perror("child sem_close");
            child_ok = 0;
        }
        _exit(child_ok ? 0 : 1);
    }

    int parent_ok = add_rounds(shared, lock, rounds, &shared->parent_adds) == 0;
    int wstatus;
    pid_t result;
    do {
        result = waitpid(pid, &wstatus, 0);    /* always reap: no zombie child */
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
        perror("waitpid");
        goto cleanup;
    }
    int child_ok = WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
    printf("Child pid %ld finished: %s\n", (long)pid, child_ok ? "exit code 0" : "FAILED");

    long expected = 2L * rounds;
    printf("Parent adds: %ld   Child adds: %ld\n", shared->parent_adds, shared->child_adds);
    printf("Shared counter = %ld, expected %ld -> %s\n", shared->counter, expected,
           shared->counter == expected ? "MATCH (no lost updates)" : "MISMATCH");
    printf("Message read from shared memory: \"%s\"\n", shared->message);
    if (parent_ok && child_ok && shared->counter == expected)
        status = 0;

cleanup:
    if (shared != MAP_FAILED) {
        if (munmap(shared, sizeof(SharedData)) == -1) {
            perror("munmap");
            status = 1;
        } else {
            puts("munmap:     mapping removed");
        }
    }
    if (fd != -1 && close(fd) == -1) {
        perror("close");
        status = 1;
    }
    if (shm_created) {
        if (shm_unlink(shm_name) == -1) {
            perror("shm_unlink");
            status = 1;
        } else {
            printf("shm_unlink: removed %s\n", shm_name);
        }
    }
    if (lock != SEM_FAILED) {
        if (sem_close(lock) == -1) {
            perror("sem_close");
            status = 1;
        } else {
            puts("sem_close:  closed semaphore handle");
        }
    }
    if (sem_created) {
        if (sem_unlink(sem_name) == -1) {
            perror("sem_unlink");
            status = 1;
        } else {
            printf("sem_unlink: removed %s\n", sem_name);
        }
    }

    /* Cleanup check: on Linux these objects are files under /dev/shm. */
    snprintf(path, sizeof(path), "/dev/shm%s", shm_name);
    int shm_left = access(path, F_OK) == 0;
    snprintf(path, sizeof(path), "/dev/shm/sem.%s", sem_name + 1);
    int sem_left = access(path, F_OK) == 0;
    printf("Cleanup check: shared memory %s, semaphore %s in /dev/shm\n",
           shm_left ? "STILL PRESENT" : "gone", sem_left ? "STILL PRESENT" : "gone");
    if (shm_left || sem_left)
        status = 1;
    return status;
}
