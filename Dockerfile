FROM debian:bookworm-slim AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake libsqlite3-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B /tmp/build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /tmp/build --parallel

FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends libsqlite3-0 curl \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --uid 10001 --home-dir /app --create-home app \
 && mkdir -p /app/data \
 && chown -R app:app /app

WORKDIR /app
COPY --from=build --chown=app:app /tmp/build/rest_server /app/rest_server
COPY --from=build --chown=app:app /src/frontend /app/frontend

USER app
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -fsS http://127.0.0.1:8080/health || exit 1
CMD ["./rest_server"]
