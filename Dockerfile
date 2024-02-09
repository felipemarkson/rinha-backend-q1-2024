FROM alpine:3.19 AS base
RUN apk update && apk upgrade && apk add liburing libpq

FROM base as builder
RUN apk add linux-headers libpq-dev liburing-dev build-base gdb
ENV DOCKER="true"
WORKDIR /code
COPY . .
RUN ./build.sh

FROM builder as runner
EXPOSE 5555
COPY --from=builder /code/main /bin/backend
ENTRYPOINT ["backend"]
