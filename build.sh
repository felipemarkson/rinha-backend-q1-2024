gcc -D_POSIX_C_SOURCE=200809L -O0 -ggdb -std=c99 -Wall -Wextra -pedantic \
-o maind -luring main.c

gcc -O3 -o main -luring main.c
strip main
