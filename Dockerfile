FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libssl3 \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -r -s /bin/false titan \
    && mkdir -p /etc/titan \
    && chown titan:titan /etc/titan

COPY --from=builder --chown=titan:titan /app/build/titan /usr/local/bin/
COPY --chown=titan:titan config.example.json /etc/titan/config.json

USER titan

EXPOSE 9001

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD nc -z localhost 9001 || exit 1

CMD ["titan", "-c", "/etc/titan/config.json"]
