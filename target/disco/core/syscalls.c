/**
 ******************************************************************************
 * @file      syscalls.c
 * @author    Auto-generated by STM32CubeIDE
 * @brief     STM32CubeIDE Minimal System calls file
 *
 *            For more information about which c-functions
 *            need which of these lowlevel functions
 *            please consult the Newlib libc-manual
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/unistd.h>

#include "filesys.h"
#include "fatfs.h"

/* Variables */
typedef FRESULT (*frw_t)(FIL *, void*, UINT, UINT*);
//#undef errno
extern int errno;
extern int __io_putchar(int ch) __attribute__((weak));
__attribute__((weak)) int __io_getchar(void) {return -1;};

/**
 * 
 *  File access functions 
 * 
 * 
 * */
int _open(char *path, int flags, ...)
{
	return FileSys_Open(path, flags);
}

int _close(int file)
{
	if(file == STDOUT_FILENO || file == STDIN_FILENO || file == STDERR_FILENO){
		errno = EBADF;
		return -1;
	}

	return FileSys_Close(file);
}

int _read(int file, char *ptr, int len)
{
	if(file == STDIN_FILENO){
		for (int DataIdx = 0; DataIdx < len; DataIdx++)
		{
			*ptr++ = (char)__io_getchar();
		}	
		return len;
	}

	return FileSys_Read(file, ptr, len);
}

int _write(int file, char *ptr, int len)
{	

	if(file == STDOUT_FILENO || file == STDERR_FILENO){
		for (int DataIdx = 0; DataIdx < len; DataIdx++)
		{
			__io_putchar(*ptr++);
		}	
		return len;
	}

	return FileSys_Write(file, ptr, len);
}

int _lseek(int file, int offset, int directive)
{
	if (file == STDOUT_FILENO){
		errno = EBADF;
		return -1;
	}

	if(offset < 0 || directive == SEEK_CUR){
		errno = EINVAL;
		return -1;
	}	

	return FileSys_Lseek(file, offset, directive);
}
/* ********************************************************** */
int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	errno = EINVAL;
	return -1;
}

void _exit (int status)
{
	_kill(status, -1);
	while (1) {}		/* Make sure we hang here */
}

int _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	return 1;
}


int _wait(int *status)
{
	errno = ECHILD;
	return -1;
}

int _unlink(char *name)
{
	errno = ENOENT;
	return -1;
}

int _times(struct tms *buf)
{
	return -1;
}

int _stat(char *file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _link(char *old, char *new)
{
	errno = EMLINK;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _execve(char *name, char **argv, char **env)
{
	errno = ENOMEM;
	return -1;
}


/*
===============================================================================

FileSys IO

===============================================================================
*/
typedef struct openedfile_s{
    FIL file;
    char path[64];
    int fd;
    struct openedfile_s *next;
}openedfile_t;

static openedfile_t openedfiles = {
    .path[0] = '\0',
    .fd = 16,			// first 16 files are reserved for system
    .next = NULL
};

static openedfile_t *getOpenedFile(int fd){
    openedfile_t *of = openedfiles.next;

    while(of != NULL){
        if(of->fd == fd){		
            return of;
        }
        of = of->next;
    }

    return NULL;
} 

/**
 * @brief Redirect from _open
 * 
 * @param path 		: File path
 * @param flags 	: Not used
 * @return int 		: File descriptor number, -1 on fail
 */
int FileSys_Open(char *path, int flags){
    openedfile_t *newof, *of;
    FRESULT fr;
    int fd;

    of = &openedfiles;   

    while(of->next != NULL){		
        of = of->next;
    }

    fd = of->fd + 1;

    newof = (openedfile_t*)malloc(sizeof(openedfile_t));

    if(flags & O_WRONLY) {
        fr = f_open(&newof->file, path, FA_WRITE);
    }else{
        fr = f_open(&newof->file, path, FA_READ);
    }    

    if (fr == FR_OK){
        //Sys_Printf("File opened: %s %d\n",path, fd);
        newof->fd = fd;
        strcpy(newof->path, path);
        newof->next = NULL;
        of->next = newof;
        return fd;
    }

    //Sys_Printf("%s: %s failed with code %d\n", __func__, path, fr);
    
    free(newof);
    
    return -1;
}

int FileSys_Close(int fd){
    openedfile_t *of = &openedfiles;

     while(of->next != NULL){
        if(of->next->fd == fd){					
            if (f_close(&of->next->file) != FR_OK){
                return -1;
            }
            //Sys_Printf("File closed: %s %d\n", of->next->path, of->next->fd);
            openedfile_t *nn = of->next->next;
            free(of->next);
            of->next = nn;
            return 0;
        }
        of = of->next;		
    }

    return -1;
}

static int filesys_read_write(int fd, char *ptr, int len, void *oper){
    UINT brw;
    openedfile_t *of = getOpenedFile(fd);

    if(of == NULL){
        return -1;
    }

    if(((frw_t)oper)(&of->file, ptr, len, &brw) != FR_OK){
        return -1;
    }

    return brw;
}

int FileSys_Read(int fd, char *ptr, int len){
    return filesys_read_write(fd, ptr, len, f_read);
}

int FileSys_Write(int fd, char *ptr, int len){
    return filesys_read_write(fd, ptr, len, f_write);
}

int FileSys_Lseek(int fd, int offset, int directive){
    openedfile_t *of = getOpenedFile(fd);

    if(of == NULL){
        return -1;
    }

    if (f_lseek(&of->file, offset) == FR_OK){
        return offset;
    }

    return -1;
}

int FileSys_GetSize(int fd){
	openedfile_t 	*of;

    of = getOpenedFile(fd);

    if(of == NULL){
        return 0;
    }

    return f_size (&of->file);
}