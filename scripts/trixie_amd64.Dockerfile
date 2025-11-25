FROM --platform=amd64 debian:trixie

WORKDIR '/build'

COPY packages-microsoft-prod.deb /build

RUN apt update -y
RUN apt install -y /build/packages-microsoft-prod.deb
COPY microsoft-prod-amd64.list /etc/apt/sources.list.d
RUN echo 'libk4a1.4 libk4a1.4/accepted-eula-hash string 0f5d5c5de396e4fee4c0753a21fee0c1ed726cf0316204edda484f08cb266d76' | debconf-set-selections
RUN echo "libk4a1.4 libk4a/accepted-eula boolean true" | debconf-set-selections
RUN apt update -y && DEBIAN_FRONTEND=noninteractive apt install -y build-essential cmake libopencv-dev libk4a1.4 libk4a1.4-dev libcurl4-openssl-dev

