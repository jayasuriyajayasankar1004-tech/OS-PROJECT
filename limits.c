/* FR7: Resource limits.
 * Reads the open-file limit (RLIMIT_NOFILE) with getrlimit(), lowers the soft
 * limit with setrlimit(), then opens /dev/null until open() fails, to show
 * the kernel enforcing the limit. Afterwards it closes everything and restores
 * the original limit. Only this process is affected. */
#define _POSIX_C_SOURCE 200809L
#include "campus.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static void print_limit(const char *label, const struct rlimit *lim)
{
    printf("%s soft = ", label);
    if (lim->rlim_cur == RLIM_INFINITY)
        printf("unlimited");
    else
        printf("%llu", (unsigned long long)lim->rlim_cur);
    printf(", hard = ");
    if (lim->rlim_max == RLIM_INFINITY)
        puts("unlimited");
    else
        printf("%llu\n", (unsigned long long)lim->rlim_max);
}

int run_limits(int new_soft)
{
    struct rlimit original, lowered, check;
    int fds[FD_LIMIT_MAX];
    int opened = 0, open_errno = 0, status = 0;

    if (getrlimit(RLIMIT_NOFILE, &original) == -1) {
        perror("getrlimit");
        return 1;
    }
    print_limit("RLIMIT_NOFILE before:", &original);
    if (original.rlim_max != RLIM_INFINITY && (rlim_t)new_soft > original.rlim_max) {
        fprintf(stderr, "limits: %d is above the hard limit; a normal user "
                "cannot raise the hard limit.\n", new_soft);
        return 1;
    }

    lowered = original;
    lowered.rlim_cur = (rlim_t)new_soft;      /* change soft only, keep hard */
    if (setrlimit(RLIMIT_NOFILE, &lowered) == -1) {
        perror("setrlimit");
        return 1;
    }
    if (getrlimit(RLIMIT_NOFILE, &check) == -1) {
        perror("getrlimit");
        status = 1;
        goto restore;
    }
    print_limit("RLIMIT_NOFILE after setrlimit:", &check);
    printf("Descriptors 0, 1, 2 (stdin, stdout, stderr) are already open.\n"
           "Opening /dev/null repeatedly until the kernel refuses...\n");

    /* fds are numbered from 0, so with soft limit N only fds 0..N-1 can exist. */
    while (opened < new_soft) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd == -1) {
            open_errno = errno;
            break;
        }
        fds[opened++] = fd;
    }
    if (opened > 0)
        printf("Opened %d files: fd %d up to fd %d\n", opened, fds[0], fds[opened - 1]);
    if (open_errno) {
        printf("Next open() FAILED: errno %d %s (%s)\n", open_errno,
               open_errno == EMFILE ? "EMFILE" : "", strerror(open_errno));
        if (open_errno == EMFILE)
            printf("Observed effect: the process hit its soft limit of %d open files, "
                   "so the kernel refused a new descriptor.\n", new_soft);
    } else {
        puts("open() never failed (unexpected).");
        status = 1;
    }

    for (int i = 0; i < opened; i++) {
        if (close(fds[i]) == -1) {
            perror("close");
            status = 1;
        }
    }
    printf("Closed the %d test descriptors.\n", opened);

restore:
    if (setrlimit(RLIMIT_NOFILE, &original) == -1) {
        perror("setrlimit (restore)");
        return 1;
    }
    if (getrlimit(RLIMIT_NOFILE, &check) == -1) {
        perror("getrlimit");
        return 1;
    }
    print_limit("RLIMIT_NOFILE restored:", &check);
    if (check.rlim_cur != original.rlim_cur || check.rlim_max != original.rlim_max) {
        fputs("Limit was NOT restored to its original value.\n", stderr);
        return 1;
    }
    puts("Limit is back to its original value, so later commands are not affected.");
    return status;
}
