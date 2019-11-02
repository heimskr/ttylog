#ifndef TTYLOG_H_
#define TTYLOG_H_

#define READ_SIZE 512

void run_child(const char *slave_name, int argc, char **argv);

struct termios getattr();
void setattr(const struct termios *attrs);
void cbreak();

ssize_t write_escaped(int fd, const char *buffer, size_t size);
ssize_t try_write(int fd, const char *buffer, size_t size);

ssize_t find_char(const char *str, char ch);

#define IGNORE_STYLES

#endif
