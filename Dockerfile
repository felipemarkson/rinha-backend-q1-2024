FROM alpine:3.19 AS base
RUN apk update && apk upgrade && apk add liburing libpq pgbouncer curl && \
    adduser -D -S localuser && chown localuser /usr/bin/pgbouncer
ENV DOCKER="true"


FROM base as builder
RUN apk add linux-headers libpq-dev liburing-dev build-base gdb
WORKDIR /code
COPY . .
RUN ./scripts/build.sh
RUN chown localuser /code/maind

FROM base as runner
EXPOSE 9999
WORKDIR /home/localuser
COPY ./configs/pgbouncer-docker.ini ./pgbouncer.ini
COPY ./configs/userslist.txt ./userslist.txt
COPY --from=builder /code/maind backend
COPY scripts/run.sh  run.sh
USER localuser
ENTRYPOINT ["/bin/sh", "-c", "./run.sh"]
