#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "ttylog.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: ttylog <program> [args]...\n");
		exit(1);
	}

	int ptmx_fd = posix_openpt(O_RDWR);
	if (ptmx_fd < 0) {
		fprintf(stderr, "Error opening pty: %s\n", strerror(errno));
		exit(1);
	}

	const char *name = ptsname(ptmx_fd);
	printf("ptmx_fd[%d], name[%s]\n", ptmx_fd, name);

	if (grantpt(ptmx_fd) < 0) {
		fprintf(stderr, "grantpt failed: %s\n", strerror(errno));
		exit(1);
	}

	if (unlockpt(ptmx_fd) < 0) {
		fprintf(stderr, "unlockpt failed: %s\n", strerror(errno));
		exit(1);
	}

	int pid = fork();
	if (pid == 0) {
		run_child(name, argc - 1, argv + 1);
		exit(0);
	} else if (pid == -1) {
		fprintf(stderr, "fork() failed: %s\n", strerror(errno));
		exit(1);
	}

	int log_fd = open(".ttylog", O_CREAT | O_APPEND | O_WRONLY, 0666);
	if (log_fd < 0) {
		fprintf(stderr, "Couldn't open .ttylog: %s\n", strerror(errno));
		exit(1);
	}

	printf("PID: %d\n", pid);
	ssize_t byte_count;

	if (fork() == 0) {
		char buffer[READ_SIZE];

		while (0 < (byte_count = read(ptmx_fd, buffer, READ_SIZE))) {
			if (write_escaped(log_fd, buffer, byte_count) < 0) {
				fprintf(stderr, "write(log_fd) failed: %s\n", strerror(errno));
				exit(1);
			}

			if (write(STDOUT_FILENO, buffer, byte_count) < 0) {
				fprintf(stderr, "write(STDOUT_FILENO) failed: %s\n", strerror(errno));
				exit(1);
			}
		}

		if (byte_count == -1) {
			fprintf(stderr, "read() failed: %s\n", strerror(errno));
			return errno;
		}
	} else {
		cbreak();
		char byte;
		while (0 < (byte_count = read(STDIN_FILENO, &byte, 1))) {
			if (write(ptmx_fd, &byte, 1) < 0) {
				fprintf(stderr, "write(ptmx_fd) failed: %s\n", strerror(errno));
				exit(1);
			}
		}

		if (byte_count == -1) {
			fprintf(stderr, "read() failed: %s\n", strerror(errno));
			return errno;
		}
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

struct termios getattr() {
	struct termios out;
	if (tcgetattr(STDIN_FILENO, &out) < 0) {
		fprintf(stderr, "tcgetattr() failed: %s\n", strerror(errno));
		exit(1);
	}

	return out;
}

void setattr(const struct termios *attrs) {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, attrs) < 0) {
		fprintf(stderr, "tcsetattr() failed: %s\n", strerror(errno));
		exit(1);
	}
}

void cbreak() {
	struct termios attrs = getattr();
	attrs.c_lflag &= ~(ECHO | ICANON | ISIG);
	attrs.c_iflag &= ~IXON;
	setattr(&attrs);
}

ssize_t write_escaped(int fd, const char *buffer, size_t size) {
	char ch;
	ssize_t written, total = 0;

	for (size_t i = 0; i < size; ++i) {
		ch = buffer[i];
		if (ch == '\x1b') {
			if (i && buffer[i] != '\n')
				write(fd, "\n", 1);

			size_t j;
			for (j = i + 2; j < size; ++j) {
				char jch = buffer[j];
				if (0x40 <= jch && jch <= 0x7e) {
					char color;
					switch (jch) {
						case 'r':
							color = 3; break; // top/bottom margins: yellow
						case 'm':
							color = 2; break; // styling: green
						case 'J': case 'K':
							color = 1; break; // erasing: red
						case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H':
							color = 6; break; // movement: cyan
						case 'S': case 'T':
							color = 4; break; // scrolling: blue
						default:  color = -1;
					}
					if (color == -1) {
						total += try_write(fd, "\x1b[2", 3);
					} else {
						total += try_write(fd, "\x1b[3", 3);
						color += '0';
						total += try_write(fd, &color, 1);
					}

					total += try_write(fd, "m^", 2);
					total += try_write(fd, buffer + i + 1, j - i);
					total += try_write(fd, "\x1b[0m", 4);
					i = j;
					j = 0;
					break;
				}
			}

			if (j == 0) {
				continue;
			} else {
				written = try_write(fd, "\x1b[2m^\x1b[22m", 10);
			}
		} else if (ch == '\r') {
			written = try_write(fd, "\x1b[2m\\r\x1b[22m", 11);
		} else if (ch == '\n') {
			written = try_write(fd, "\x1b[2m\\n\x1b[22m\n", 12);
		} else {
			written = try_write(fd, &ch, 1);
		}

		if (written == 0)
			break;

		total += written;
	}

	return total;
}

ssize_t try_write(int fd, const char *buffer, size_t size) {
	ssize_t written = write(fd, buffer, size);
	if (written == -1) {
		fprintf(stderr, "write(%d) failed: %s\n", fd, strerror(errno));
		exit(1);
	}

	return written;
}
