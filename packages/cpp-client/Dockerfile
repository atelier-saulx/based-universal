FROM openjdk:11.0.9-jdk

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends --no-upgrade build-essential \
    libc++-dev \
    libcurl4-openssl-dev \
    libz-dev \
    libssl-dev
    # cmake


WORKDIR /build
COPY ./src ./src
COPY ./lib ./lib
COPY ./include ./include
COPY ./example ./example
# COPY ./build-scripts ./build-scripts
# COPY ./CMakeLists.txt ./CMakeLists.txt
COPY ./Makefile .
# CMD ls .
CMD make && cp libbased.so ./out/libbased.so
# CMD make linked-example && cp example ./out/example
# CMD cd build-scripts && bash ./build-linux.sh && cp -r build_linux/Release/ ./out/
