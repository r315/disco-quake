#ifndef __FILESYS_H
#define __FILESYS_H

int FileSys_Open(char *path, int flags);
int FileSys_Read(int fd, char *ptr, int len);
int FileSys_Write(int fd, char *ptr, int len);
int FileSys_Close(int fd);
int FileSys_Lseek(int fd, int offset, int directive);
int FileSys_GetSize(int fd);

#endif