#ifndef USERPROG_USER_IO_H
#define USERPROG_USER_IO_H

void user_io_init      (void);
void user_io_block     (void);
void user_io_release   (void);
void user_io_close_all (void);
int  user_io_open      (const char *file);
int  user_io_filesize  (int fd);
int  user_io_read      (int fd, void *buf, unsigned size);
int  user_io_write     (int fd, const void *buf, unsigned size);
void user_io_seek      (int fd, unsigned position);
unsigned user_io_tell  (int fd);
void user_io_close     (int fd);

#endif /* USERPROG_USER_IO_H */
