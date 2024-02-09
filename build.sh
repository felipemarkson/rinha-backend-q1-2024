set -x
COMPILE_DBG="gcc -D_POSIX_C_SOURCE=200809L -O0 -ggdb -std=c99 -Wall -Wextra -Wswitch-enum -pedantic"
COMPILE="gcc -D_POSIX_C_SOURCE=200809L -O3 -g0"
if [[ "true" == "${DOCKER}" ]]; then
    COMPILE="$COMPILE -DDOCKER"
    COMPILE_DBG="$COMPILE_DBG -DDOCKER"
fi

gcc -Ipicoparser -O3 -o objs/picohttpparser.o -c picohttpparser/picohttpparser.c
gcc -IcJSON -O3 -o objs/cJSON.o -c cJSON/cJSON.c


$COMPILE_DBG -o objs/server_d.o -c server.c
$COMPILE_DBG -o objs/request_handler_d.o -c request_handler.c
$COMPILE_DBG -o objs/main_d.o -c main.c
$COMPILE_DBG objs/cJSON.o objs/picohttpparser.o objs/*_d.o -o maind -luring -lpq

$COMPILE -o objs/server_r.o -c server.c
$COMPILE -o objs/request_handler_r.o -c request_handler.c
$COMPILE -o objs/main_r.o -c main.c
$COMPILE objs/cJSON.o objs/picohttpparser.o objs/*_r.o -o main -luring -lpq
strip main