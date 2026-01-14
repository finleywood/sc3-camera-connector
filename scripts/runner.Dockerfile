ARG ARCH=x86

############################
# x86 build stage
############################

FROM --platform=linux/amd64 ghcr.io/finleywood/sc3-camera-connector/builder:latest AS build-x86
FROM --platform=linux/arm64 ghcr.io/finleywood/sc3-camera-connector/builder-arm:latest AS build-arm64

############################
# Select build stage dynamically
############################
FROM build-${ARCH} AS build

RUN mkdir -p /build
WORKDIR /build

COPY src src
COPY include include
COPY auxiliary auxiliary
COPY CMakeLists.txt .

RUN mkdir -p build

WORKDIR /build/build

RUN cmake .. && make

FROM build-${ARCH} AS runtime

WORKDIR /app

COPY --from=build /build/build/SC3-Camera-Connector /usr/bin/
RUN chmod +x /usr/bin/SC3-Camera-Connector

RUN mkdir -p /etc/sc3
COPY --from=build /build/auxiliary/config.json /etc/sc3/config.json

RUN mkdir -p /run/sc3

RUN apt update -y && apt install socat -y

COPY scripts/run_in_docker.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

EXPOSE 8080

ENTRYPOINT ["/app/entrypoint.sh"]
