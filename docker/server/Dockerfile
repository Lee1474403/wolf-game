FROM alpine:3.20 AS builder

RUN apk add --no-cache build-base cmake ninja

WORKDIR /src/server
COPY server/CMakeLists.txt ./CMakeLists.txt
COPY server/include ./include
COPY server/src ./src

RUN cmake -S . -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DBUILD_LEGACY_CLI_CLIENT=OFF \
    && cmake --build /build --target server --parallel \
    && strip /build/server

FROM alpine:3.20 AS runtime

RUN apk add --no-cache libstdc++ tzdata \
    && addgroup -S -g 10001 wolfgame \
    && adduser -S -D -H -u 10001 -G wolfgame wolfgame \
    && mkdir -p /var/log/werewolf \
    && chown -R wolfgame:wolfgame /var/log/werewolf

COPY --from=builder --chown=wolfgame:wolfgame /build/server /usr/local/bin/werewolf-server

ENV GAME_PORT=8888 \
    GAME_BACKLOG=128 \
    GAME_MAX_CONNECTIONS=1024 \
    GAME_MAX_ROOMS=0 \
    GAME_PHASE_SECONDS=60 \
    GAME_DISCUSSION_SECONDS=60 \
    GAME_READY_TIMEOUT_SECONDS=300 \
    GAME_LOG_FILE=/var/log/werewolf/server.log \
    TZ=Asia/Shanghai

USER wolfgame
VOLUME ["/var/log/werewolf"]
EXPOSE 8888/tcp

HEALTHCHECK --interval=20s --timeout=3s --start-period=5s --retries=3 \
    CMD nc -z 127.0.0.1 "$GAME_PORT" || exit 1

ENTRYPOINT ["/usr/local/bin/werewolf-server"]
