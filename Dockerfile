FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir --break-system-packages conan

WORKDIR /app

# The host lockfile is platform-specific. Resolve a Linux graph in the image so
# Linux-only transitive dependencies (for example Boost's libbacktrace) are included.
COPY conanfile.py ./

RUN conan profile detect --force \
    && conan install . --output-folder=build --build=missing -s build_type=Release

COPY CMakeLists.txt ./
COPY src ./src

RUN cmake -S . -B build \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target load_balancer

RUN useradd --system --create-home --home-dir /data appuser

WORKDIR /data
USER appuser

EXPOSE 3000
VOLUME ["/data"]

# Conan's runtime environment makes any shared dependency libraries available.
CMD ["/bin/sh", "-c", ". /app/build/conanrun.sh && exec /app/build/load_balancer"]
