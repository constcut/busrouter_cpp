
#ifndef MEMCHECKER
#define MEMCHECKER

#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>

inline size_t getRAM() {
    long rss = 0L;
    FILE* fp = NULL;
    if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
        return (size_t)0L;     
    if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
    {
        fclose( fp );
        return (size_t)0L;
    }
    fclose( fp );
    return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);
}

#endif