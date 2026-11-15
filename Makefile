CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SOURCES = main.c jobs.c scheduler.c logger.c banker.c worker.c limits.c ipc.c demo.c

# glibc 2.34+ has shm_open built in; older versions (e.g. Ubuntu 20.04) need -lrt.
# Try linking a tiny shm_open program without it; add -lrt only if that fails.
RT_LIB := $(shell echo 'int main(void){return shm_open("/x",0,0);}' \
            | $(CC) -include sys/mman.h -x c - -o /dev/null 2>/dev/null || echo -lrt)

campuscompute: $(SOURCES) campus.h
	$(CC) $(CFLAGS) -pthread $(SOURCES) -o $@ $(RT_LIB)

.PHONY: clean check-clean
# Only files this project creates: the program and the logger's output.
clean:
	$(RM) campuscompute activity.log

# Looks for anything this project left behind. It only lists, never deletes.
check-clean:
	@echo "campuscompute processes still running:"
	@pgrep -a -x campuscompute || echo "  none"
	@echo "campuscompute objects in /dev/shm:"
	@ls /dev/shm | grep campuscompute || echo "  none"
