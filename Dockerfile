FROM alpine:3.19 AS base
RUN apk update && apk upgrade && apk add liburing libpq pgbouncer && \
    adduser -D -S localuser && chown localuser /usr/bin/pgbouncer


FROM base as builder
RUN apk add linux-headers libpq-dev liburing-dev build-base gdb
ENV DOCKER="true"
ENV CHGMAXCONN="true"
WORKDIR /code
COPY . .
RUN ./build.sh
RUN chown localuser /code/maind

FROM base as runner
EXPOSE 9999
WORKDIR /home/localuser
COPY ./pgbouncer-docker.ini ./pgbouncer.ini
COPY ./userslist.txt ./userslist.txt
COPY --from=builder /code/maind backend
USER localuser
CMD pgbouncer pgbouncer.ini -d; ./backend
