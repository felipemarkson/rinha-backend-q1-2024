#ifndef RESPONSE_H
#define RESPONSE_H

#include <string.h>


#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#define SIZE_ARRAY(static_array) sizeof(static_array) / sizeof(*(static_array))

#define SET_STATIC_RESPONSE(buffer, response) memcpy((buffer), response, SIZE_ARRAY(response))

#define BADREQUEST                 \
    "HTTP/1.1 400 Bad Request\r\n" \
    "File: " __FILE__    "\r\n"    \
    "Line: " LINE_STRING "\r\n"    \
    "Connection: close\r\n"        \
    "Content-type: text/plain\r\n" \
    "Content-Length: 0\r\n"        \
    "\r\n"

#define INTERNALERROR                 \
    "HTTP/1.1 500 Internal Error\r\n" \
    "File: " __FILE__     "\r\n"      \
    "Line: " LINE_STRING  "\r\n"      \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 0\r\n"           \
    "\r\n"

#define NOTFOUND                      \
    "HTTP/1.1 404 Not found\r\n"      \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 0\r\n"           \
    "\r\n"

#define UNPROCESSABLE                       \
    "HTTP/1.1 422 Unprocessable Entity\r\n" \
    "File: " __FILE__     "\r\n"            \
    "Line: " LINE_STRING  "\r\n"            \
    "Connection: close\r\n"                 \
    "Content-type: text/plain\r\n"          \
    "Content-Length: 0\r\n"                 \
    "\r\n"


#define OK_TRANSACAO                  \
    "HTTP/1.1 200 OK\r\n"             \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: %d\r\n"          \
    "\r\n"                            \
    "%s\n"

#define OK_EXTRATO                    \
    "HTTP/1.1 200 OK\r\n"             \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: %d\r\n"          \
    "\r\n"                            \
    "%s\n"

#define OK_EXIT                       \
    "HTTP/1.1 200 OK\r\n"             \
    "File: " __FILE__     "\r\n"      \
    "Line: " LINE_STRING  "\r\n"      \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 0\r\n"           \
    "\r\n"

#define OK_PING                       \
    "HTTP/1.1 200 PONG\r\n"           \
    "\r\n"

#endif
