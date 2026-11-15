# CampusCompute

A C command-line prototype for Linux that demonstrates job scheduling, a concurrent activity logger, shared-memory IPC with a named semaphore, the Banker's safety algorithm, worker failure detection and process resource limits.

Operating Systems (UE25CS242A) Level 3 mini-project.

| Member | Name | SRN | Part |
| --- | --- | --- | --- |
| Person A | Hrishikesh Gangatkar | PES1UG25CS210 | Jobs and scheduling (FR1, FR2), logger (FR3): `jobs.c`, `scheduler.c`, `logger.c`, `main.c`, `campus.h`, `jobs.txt`, `Makefile` |
| Person B | Jayasuriya Jayasankar | PES1UG25CS225 | IPC (FR4), Banker's algorithm (FR5), workers (FR6), resource limits (FR7), integrated demo: `ipc.c`, `banker.c`, `worker.c`, `limits.c`, `demo.c`, `banker_safe.txt`, `banker_unsafe.txt` |

## Prerequisites

- Ubuntu or WSL Ubuntu
- GCC, GNU Make and POSIX threads. If GCC or Make is missing, install them with `sudo apt install build-essential`.
- `zip` and `unzip`, only if you need to make the submission archive

## Files

| File | Purpose |
| --- | --- |
| `main.c` | Command-line interface: reads the command, checks its arguments and calls the right module |
| `campus.h` | Shared header: the job structure and every module's function declarations |
| `jobs.c` | Reads and validates `jobs.txt` |
| `scheduler.c` | FCFS, Round Robin and non-preemptive Priority scheduling |
| `logger.c` | Producer-consumer logger using POSIX threads |
| `ipc.c` | POSIX shared memory and a named semaphore between two processes |
| `banker.c` | Banker's safety algorithm |
| `worker.c` | Child workers with `fork()` and failure detection with `waitpid()` |
| `limits.c` | Open-file limit with `getrlimit()`/`setrlimit()` |
| `demo.c` | Runs every module in one go and prints a summary |
| `jobs.txt` | Sample job set |
| `banker_safe.txt`, `banker_unsafe.txt` | Banker test inputs, one safe and one unsafe |
| `Makefile` | Builds the program (`make`), removes what it creates (`make clean`) and checks for leftovers (`make check-clean`) |

## Build

```sh
make
```

This compiles every source file with `-std=c11 -Wall -Wextra -Wpedantic` into one program, `campuscompute`. The Makefile adds `-lrt` only on older glibc versions that need it for `shm_open`.

## Run

```sh
./campuscompute --help                          # lists every command
./campuscompute fcfs jobs.txt                   # FCFS schedule and metrics
./campuscompute rr jobs.txt 2                   # Round Robin with time quantum 2
./campuscompute priority jobs.txt               # non-preemptive Priority
./campuscompute compare jobs.txt 2              # all three policies on the same jobs
./campuscompute logger jobs.txt                 # producer-consumer logger, writes activity.log
cat activity.log
./campuscompute ipc 50000                       # shared counter must equal 100000
./campuscompute banker banker_safe.txt          # prints Need table, safe sequence
./campuscompute banker banker_unsafe.txt        # prints UNSAFE and the stuck processes
./campuscompute workers                         # one normal exit, two SIMULATED failures
./campuscompute limits 8                        # open() fails with EMFILE, limit restored
./campuscompute demo jobs.txt banker_safe.txt   # all modules in one run, then a summary
```

Each feature can be run and tested on its own. Invalid commands and malformed input print a usage or error message and exit with a non-zero status, for example:

```sh
./campuscompute wrongcommand                    # prints the usage list
./campuscompute rr jobs.txt 0                   # quantum must be 1 to 100000
./campuscompute banker missing.txt              # file cannot be opened
./campuscompute limits 999                      # N must be 4 to 256
```

## Cleanup

```sh
make check-clean    # lists leftover campuscompute processes or /dev/shm objects; it never deletes anything
make clean          # removes only the files this project creates: campuscompute and activity.log
```

The program cleans up its own resources as it runs. Threads are joined, child processes are reaped, and shared memory and semaphores are unlinked. It never touches files or IPC objects that it did not create.

## Jobs and scheduling

Each line in `jobs.txt` contains four integers in this order.

```text
ID  arrival_time  CPU_burst  priority
```

The file has data rows only. Job IDs must be unique. ID and burst start at 1. Arrival and priority can start at 0. Each value is at most 100000. The program accepts up to 64 jobs. Times are in simulated CPU units.

- FCFS runs the earliest arrival to completion. Simultaneous arrivals enter in increasing job ID order.
- Round Robin takes the first ready job and gives it up to one quantum. Arrivals during the slice, including at its end, enter the queue before the unfinished job goes to the back.
- Priority picks the ready job with the smallest priority number and runs it to completion. Ties go to the earlier arrival, then the smaller ID.

CT is the time the job finishes. Turnaround time (TAT) is CT minus arrival time. Waiting time (WT) is TAT minus CPU burst. For example, in the supplied FCFS run, J2 arrives at time 1, starts at 7 and finishes at 10. Its TAT is 9 and its WT is 6.

## Logger

The producer runs the FCFS scheduler and adds ready, running and completed events to a four-slot queue. The consumer takes events in order and writes them to `activity.log`. A mutex protects the queue. The producer waits when it is full and the consumer waits when it is empty. Once the producer finishes, the consumer drains the queue. The main thread joins both threads before closing the file. Each logger run replaces `activity.log` in the current directory.

## Shared memory and semaphore

Each run creates `/campuscompute_shm_<pid>` and `/campuscompute_sem_<pid>` with `O_EXCL`, so it only ever touches objects it created itself. The parent and child each add 1 to a counter in shared memory ROUNDS times. The semaphore starts at 1 and works as a lock around each update, so the final value is exactly 2×ROUNDS. Cleanup always runs, even after an error: `munmap`, `shm_unlink`, `sem_close` and `sem_unlink`. Then the program checks that both names have gone from `/dev/shm`.

## Banker file format

Blank lines are ignored, and `#` starts a comment. The first data line is `PROCESSES RESOURCES`, with up to 10 of each. Next comes one Available row, then one Allocation row per process, then one Maximum row per process. Each row must have exactly RESOURCES whole numbers. Errors give the file name and line number. Need is Maximum minus Allocation. A process whose Need fits within Work can finish and return its Allocation to Work. If every process can finish, the order they finish in is a safe sequence. Otherwise the state is UNSAFE. Both results exit with code 0, and invalid input exits with code 1.

## Workers

The parent calls `fork()` three times. Worker 1 does real work and exits with code 0. Worker 2 is a **simulated** failure that exits with code 42. Worker 3 is a **simulated** crash that sends itself `SIGKILL`. These failures are deliberate and are labelled SIMULATED in the output; they are not real OS failures. The parent reaps each worker with `waitpid()`. It then uses `WIFEXITED`/`WEXITSTATUS` to recognise normal and error exits, and `WIFSIGNALED`/`WTERMSIG` to recognise death by a signal. The order of the children's own lines can differ between runs, because the processes run at the same time.

## Resource limits

`getrlimit(RLIMIT_NOFILE)` reads the soft and hard limits on open files. `setrlimit` lowers only the soft limit, which a normal user is allowed to do. The program then opens `/dev/null` until `open()` fails with `EMFILE` ("Too many open files"). File descriptors 0, 1 and 2 are already in use, so N−3 opens succeed. All test descriptors are then closed, the original limit is restored, and the program checks that the limit really was restored.

## Integrated demo

`demo JOBS_FILE BANKER_FILE` runs FCFS, Round Robin (quantum 2), Priority, the logger, Banker, workers, limits (N = 8) and IPC (20000 rounds) in that order. The final summary is built from each step's real return value and the scheduler's average waiting times.

## Screenshots

Before taking screenshots, set the terminal prompt to show your own full SRN:

```sh
export SRN='PES1UG25CS225'; export PS1='[$SRN] \w\$ '
```
