COMPILE_DBG="gcc -D_POSIX_C_SOURCE=200809L -O0 -ggdb -std=c99 -Wall -Wextra -Wswitch-enum -pedantic"

gcc -Ipicoparser -O3 -o objs/picohttpparser.o -c picohttpparser/picohttpparser.c
gcc -IcJSON -O3 -o objs/cJSON.o -c cJSON/cJSON.c


$COMPILE_DBG -o objs/server.o -c server.c
$COMPILE_DBG -o objs/request_handler.o -c request_handler.c
$COMPILE_DBG -o objs/main.o -c main.c
$COMPILE_DBG objs/*.o -o main -luring -lbsd -lpq