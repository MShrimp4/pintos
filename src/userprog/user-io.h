#ifndef USERPROG_USER_IO_H
#define USERPROG_USER_IO_H

#include <stdbool.h>

void user_io_init      (void);
void user_io_block     (void);
void user_io_release   (void);
void user_io_close_all (void);
bool user_io_create    (const char *file, unsigned initial_size);
bool user_io_remove    (const char *file);
int  user_io_open      (const char *file);
int  user_io_filesize  (int fd);
int  user_io_read      (int fd, void *buf, unsigned size);
int  user_io_write     (int fd, const void *buf, unsigned size);
void user_io_seek      (int fd, unsigned position);
unsigned user_io_tell  (int fd);
void user_io_close     (int fd);

void user_io_deny_write (int fd);

#ifdef VM
int  user_io_mmap (int fd, void *addr);
void user_io_munmap (int mid);
#endif /* VM */

#endif /* USERPROG_USER_IO_H */
