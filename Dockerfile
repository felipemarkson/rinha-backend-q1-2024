FROM alpine:3.19 AS base
RUN apk update && apk upgrade && apk add liburing libpq curl
ENV DOCKER="true"


FROM base as builder
RUN apk add linux-headers libpq-dev liburing-dev build-base gdb
WORKDIR /code
COPY . .
RUN ./scripts/build.sh

FROM base as runner
COPY scripts/run.sh /run.sh
COPY --from=builder /code/maind /bin/backend
ENTRYPOINT ["sh", "-c", "./run.sh"]
