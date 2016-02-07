#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

int _close(int file)
{
	return -1;
}


#define ITM_Port8(n)    (*((volatile unsigned char *)(0xE0000000+4*n)))
#define ITM_Port16(n)   (*((volatile unsigned short*)(0xE0000000+4*n)))
#define ITM_Port32(n)   (*((volatile unsigned long *)(0xE0000000+4*n)))

#define DEMCR           (*((volatile unsigned long *)(0xE000EDFC)))
#define TRCENA          0x01000000


int _write(int file, char *ptr, int len)
{
	int todo;

	for (todo = 0; todo < len; todo++)
	{

	    if (DEMCR & TRCENA)
	    {
	        while (ITM_Port32(0) == 0);
	        ITM_Port8(0) = ptr[todo];
	    }

	}
	return len;
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


int _read(int file, char *ptr, int len)
{
	return 0;
}

int _lseek(int file, int ptr, int dir)
{
	return 0;
}

int _kill(int pid, int sig)
{
	(void)pid;
	(void)sig; /* avoid warnings */
	errno = EINVAL;
	return -1;
}

void _exit(int status)
{
	printf("_exit called with parameter %d\n", status);
	while(1) {;}
}

int _getpid(void)
{
	return 1;
}

/*
extern char _end;
static char *heap_end;

char* get_heap_end(void)
{
	return (char*) heap_end;
}

char* get_stack_top(void)
{
	//return (char*) __get_MSP();
	//return (char*) __get_PSP();
}

caddr_t _sbrk(int incr)
{
	char *prev_heap_end;
	if (heap_end == 0) {
		heap_end = &_end;
	}
	prev_heap_end = heap_end;
#if 1
	if (heap_end + incr > get_stack_top()) {
		printf("Heap and stack collision\n");
		for(;;);
	}
#endif
	heap_end += incr;
	return (caddr_t) prev_heap_end;
}
*/

caddr_t _sbrk(int incr) {
    extern char _end;
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0) {

        heap_end = &_end;
    }

    prev_heap_end = heap_end;
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}


