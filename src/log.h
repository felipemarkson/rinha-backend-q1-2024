#ifndef LOG_H
#define LOG_H
#include <stdint.h>
#include <stdio.h>

#ifndef LOG
#define LOG(template, ...) printf("LOG %s:%d -> " template "\n", __FILE__, __LINE__, __VA_ARGS__)
#endif

#ifndef DIE_ON_ERROR
#define LOGERR(template, ...) do{ fprintf(stderr,"ERROR %s:%d -> " template "\n", __FILE__, __LINE__, __VA_ARGS__); fflush(stderr); } while(0)
#else
    #define LOGERR(template, ...) do{ fprintf(stderr,"ERROR %s:%d -> " template "\n", __FILE__, __LINE__, __VA_ARGS__); fflush(stderr); exit(1); } while(0) 
#endif

#define FATAL() do{fprintf(stderr, "FATAL IN %s:%d -> %s \n", __FILE__, __LINE__, strerror(errno)); fflush(stderr); exit(1);} while(0)
#define FATAL2(err_io) do{fprintf(stderr, "FATAL IN %s:%d -> (%d): %s \n", __FILE__, __LINE__, -(int)(err_io), strerror(-(int)(err_io))); fflush(stderr);  exit(1);} while(0)
#define FATAL3(template, ...) do{ fprintf(stderr,"FATAL IN %s:%d -> " template "\n", __FILE__, __LINE__, __VA_ARGS__); fflush(stderr); fflush(stderr);  exit(1);  } while(0)

#define SIZE_ARRAY(static_array) sizeof(static_array) / sizeof(*(static_array))
#define STRINGFY(a) _STRINGFY(a)
#define _STRINGFY(a) #a

#define SIZE_UINTMAX_STR SIZE_ARRAY(STRINGFY(__UINT64_MAX__))

#endif