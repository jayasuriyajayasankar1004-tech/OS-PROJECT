/* FR5: Banker's safety algorithm.
 * Reads Available, Allocation and Maximum from a text file, computes
 * Need = Maximum - Allocation, and prints a safe sequence or UNSAFE.
 *
 * File format (blank lines are ignored, '#' starts a comment):
 *   PROCESSES RESOURCES        e.g. "5 3"
 *   Available row              RESOURCES numbers
 *   Allocation rows            one row per process
 *   Maximum rows               one row per process */
#define _POSIX_C_SOURCE 200809L
#include "campus.h"
#include <stdio.h>
#include <string.h>

#define MAX_PROCS 10
#define MAX_RES 10
#define LINE_SIZE 256

typedef struct {
    int procs, res;
    int available[MAX_RES];
    int allocation[MAX_PROCS][MAX_RES];
    int maximum[MAX_PROCS][MAX_RES];
    int need[MAX_PROCS][MAX_RES];
} BankerState;

/* Where we are in the input file, so errors can say "file:line:". */
typedef struct {
    FILE *file;
    const char *path;
    int line;
} Reader;

/* Reads the next line that has data on it and converts its words to numbers.
 * Stores at most 'capacity' numbers in values[] but counts all of them.
 * Returns how many numbers the line had, 0 at end of file, -1 on error. */
static int read_data_line(Reader *in, int values[], int capacity)
{
    char line[LINE_SIZE];
    while (fgets(line, sizeof(line), in->file)) {
        in->line++;
        if (!strchr(line, '\n') && !feof(in->file)) {
            fprintf(stderr, "%s:%d: line too long\n", in->path, in->line);
            return -1;
        }
        char *comment = strchr(line, '#');
        if (comment)
            *comment = '\0';                  /* ignore everything after '#' */

        int count = 0;
        for (char *word = strtok(line, " \t\r\n"); word; word = strtok(NULL, " \t\r\n")) {
            int value;
            if (!read_number(word, 0, &value)) {      /* Person A's checked parser */
                fprintf(stderr, "%s:%d: '%s' is not a whole number from 0 to %d\n",
                        in->path, in->line, word, MAX_VALUE);
                return -1;
            }
            if (count < capacity)
                values[count] = value;
            count++;
        }
        if (count > 0)
            return count;                     /* blank or comment-only: keep reading */
    }
    if (ferror(in->file)) {
        perror(in->path);
        return -1;
    }
    return 0;
}

/* Reads one row that must contain exactly 'expected' numbers. Returns 1 if ok. */
static int read_row(Reader *in, const char *what, int row[], int expected)
{
    int count = read_data_line(in, row, expected);
    if (count < 0)
        return 0;
    if (count == 0) {
        fprintf(stderr, "%s: file ended early, expected %s (%d numbers)\n",
                in->path, what, expected);
        return 0;
    }
    if (count != expected) {
        fprintf(stderr, "%s:%d: %s needs %d numbers, found %d\n",
                in->path, in->line, what, expected, count);
        return 0;
    }
    return 1;
}

/* Fills the state from the file and checks it. Returns 1 if the file is valid. */
static int load_state(const char *path, BankerState *s)
{
    Reader in = { fopen(path, "r"), path, 0 };
    if (!in.file) {
        perror(path);
        return 0;
    }
    int ok = 0, header[2], extra[1];
    char what[40];

    int count = read_data_line(&in, header, 2);
    if (count == 0) {
        fprintf(stderr, "%s: file is empty, expected 'PROCESSES RESOURCES' first\n", path);
        goto done;
    }
    if (count < 0)
        goto done;
    if (count != 2 || header[0] < 1 || header[0] > MAX_PROCS ||
        header[1] < 1 || header[1] > MAX_RES) {
        fprintf(stderr, "%s:%d: first line must be 'PROCESSES RESOURCES' "
                "(1-%d processes, 1-%d resources)\n", path, in.line, MAX_PROCS, MAX_RES);
        goto done;
    }
    s->procs = header[0];
    s->res = header[1];

    if (!read_row(&in, "Available", s->available, s->res))
        goto done;
    for (int i = 0; i < s->procs; i++) {
        snprintf(what, sizeof(what), "Allocation row of P%d", i);
        if (!read_row(&in, what, s->allocation[i], s->res))
            goto done;
    }
    for (int i = 0; i < s->procs; i++) {
        snprintf(what, sizeof(what), "Maximum row of P%d", i);
        if (!read_row(&in, what, s->maximum[i], s->res))
            goto done;
    }
    count = read_data_line(&in, extra, 1);
    if (count < 0)
        goto done;
    if (count > 0) {
        fprintf(stderr, "%s:%d: extra data after the Maximum rows "
                "(header says %d processes)\n", path, in.line, s->procs);
        goto done;
    }

    /* Need = Maximum - Allocation. A process can never hold more than its maximum. */
    for (int i = 0; i < s->procs; i++) {
        for (int j = 0; j < s->res; j++) {
            if (s->allocation[i][j] > s->maximum[i][j]) {
                fprintf(stderr, "%s: P%d holds %d of R%d but its maximum is %d "
                        "(Allocation must be <= Maximum)\n",
                        path, i, s->allocation[i][j], j, s->maximum[i][j]);
                goto done;
            }
            s->need[i][j] = s->maximum[i][j] - s->allocation[i][j];
        }
    }
    ok = 1;
done:
    fclose(in.file);
    return ok;
}

static void print_row(const int row[], int count)
{
    for (int j = 0; j < count; j++)
        printf("%3d", row[j]);
}

/* 1 if every Need[j] <= Work[j], meaning the process could finish right now. */
static int can_finish(const int need[], const int work[], int count)
{
    for (int j = 0; j < count; j++)
        if (need[j] > work[j])
            return 0;
    return 1;
}

static void print_tables(const BankerState *s, const char *path)
{
    printf("Banker's safety check: %d processes, %d resource types (%s)\n",
           s->procs, s->res, path);
    printf("Available:");
    print_row(s->available, s->res);
    printf("\n%-5s %-*s %-*s %s\n", "Proc", 3 * s->res + 2, "Allocation",
           3 * s->res + 2, "Maximum", "Need (= Max - Alloc)");
    for (int i = 0; i < s->procs; i++) {
        printf("P%-4d", i);
        print_row(s->allocation[i], s->res);
        printf("   ");
        print_row(s->maximum[i], s->res);
        printf("   ");
        print_row(s->need[i], s->res);
        putchar('\n');
    }
}

int run_banker(const char *path)
{
    BankerState s;
    if (!load_state(path, &s))
        return 1;
    print_tables(&s, path);

    /* Safety algorithm. Work starts as what is free now.
     * Repeatedly find an unfinished process whose Need fits in Work,
     * let it finish, and add its Allocation back to Work. */
    int work[MAX_RES], finished[MAX_PROCS] = {0}, sequence[MAX_PROCS], done = 0;
    memcpy(work, s.available, sizeof(work));
    puts("Steps:");
    int progress = 1;
    while (progress) {                       /* stop after a pass where nobody finished */
        progress = 0;
        for (int i = 0; i < s.procs; i++) {
            if (finished[i] || !can_finish(s.need[i], work, s.res))
                continue;
            printf("  P%d: Need", i);
            print_row(s.need[i], s.res);
            printf(" <= Work");
            print_row(work, s.res);
            for (int j = 0; j < s.res; j++)
                work[j] += s.allocation[i][j];
            printf("  -> finishes, Work becomes");
            print_row(work, s.res);
            putchar('\n');
            finished[i] = 1;
            sequence[done++] = i;
            progress = 1;
        }
    }
    if (done == 0)
        puts("  (no process's Need fits in Available)");

    if (done == s.procs) {
        printf("Result: SAFE. Safe sequence: ");
        for (int k = 0; k < done; k++)
            printf("P%d%s", sequence[k], k + 1 < done ? " -> " : "\n");
    } else {
        printf("Result: UNSAFE. With Work");
        print_row(work, s.res);
        puts(" these processes can never finish:");
        for (int i = 0; i < s.procs; i++) {
            if (finished[i])
                continue;
            printf("  P%d still needs", i);
            print_row(s.need[i], s.res);
            putchar('\n');
        }
    }
    return done == s.procs ? 0 : 2;
}
