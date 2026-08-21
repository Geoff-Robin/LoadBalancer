FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages conan

WORKDIR /app

COPY . .

RUN conan profile detect --force
RUN conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release

RUN cmake -S . -B build \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build

EXPOSE 3000

CMD ["./build/load_balancer"]
