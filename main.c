#include "request_handler.h"

int main(int argc, char const *argv[])
{
    (void) argc;
    (void) argv;
    server(request_handler);
    return 0;
}
