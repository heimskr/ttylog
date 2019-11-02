#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ttylog.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: ttylog <program> [args]...\n");
		exit(1);
	}

	int ptmx_fd = open("/dev/ptmx", O_RDWR);
	const char *name = ptsname(ptmx_fd);
	printf("ptmx_fd[%d], name[%s]\n", ptmx_fd, name);

	int pid = fork();
	if (pid == 0) {
		run_child(name, argc - 1, argv + 1);
		exit(0);
	}

	int log_fd = open(".ttylog", O_CREAT | O_APPEND | O_WRONLY);
	char buffer[READ_SIZE];
	ssize_t byte_count;

	printf("PID: %d\n", pid);
	while (0 < (byte_count = read(ptmx_fd, buffer, READ_SIZE))) {
		printf("read %zd byte(s)\n", byte_count);
		write(log_fd, buffer, byte_count);
	}

	if (byte_count == -1) {
		fprintf(stderr, "read() failed: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

void run_child(const char *slave_name, int argc, char **argv) {
	printf("run_child(\"%s\")\n", slave_name);
	printf("%s", argv[0]);
	for (int i = 1; i < argc; ++i)
		printf(" %s", argv[i]);
	printf("\n");

	for (int i = 0; i <= 2; ++i)
		fcntl(i, F_SETFD, fcntl(i, F_GETFD, 0) | FD_CLOEXEC);

	int slave_fd = open(slave_name, O_RDWR);
	close(0);
	close(1);
	close(2);
	if (dup(slave_fd) == -1) {
		fprintf(stderr, "dup 0 failed: %s\n", strerror(errno));
		exit(1);
	}

	if (dup(slave_fd) == -1) {
		fprintf(stderr, "dup 1 failed: %s\n", strerror(errno));
		exit(1);
	}

	if (dup(slave_fd) == -1) {
		fprintf(stderr, "dup 2 failed: %s\n", strerror(errno));
		exit(1);
	}

	setsid();
	execvp(argv[0], argv + 1);
}
