set -x
COMPILE_DBG="gcc -DDEBUG -D_POSIX_C_SOURCE=200809L -O3 -ggdb -std=c99 -Wall -Wextra -Wswitch-enum -pedantic"
if [[ "true" == "${DOCKER}" ]]; then
    COMPILE="$COMPILE -DDOCKER"
    COMPILE_DBG="$COMPILE_DBG -DLOG(...) -DDOCKER"
fi

rm -f objs/*.o

gcc -Ipicoparser -O3 -o objs/picohttpparser.o -c picohttpparser/picohttpparser.c
gcc -IcJSON -O3 -o objs/cJSON.o -c cJSON/cJSON.c

$COMPILE_DBG -o objs/server_d.o -c src/server.c
$COMPILE_DBG -o objs/request_d.o -c src/request.c
$COMPILE_DBG -o objs/db_d.o -c src/db.c
$COMPILE_DBG -o objs/main_d.o -c src/main.c
$COMPILE_DBG objs/cJSON.o objs/picohttpparser.o objs/*_d.o -o maind -luring -lpq