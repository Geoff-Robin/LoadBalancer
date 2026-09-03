BUILD_DIR ?= build
CONFIG ?= Release
IMAGE ?= load-balancer
CONTAINER ?= load-balancer
PORT ?= 3000
DATA_VOLUME ?= load-balancer-data

.PHONY: help configure dev-build dev-run format docker-build docker-run

help:
	@echo Available targets:
	@echo "  configure    Install Conan dependencies and configure CMake"
	@echo "  dev-build    Build the development executable"
	@echo "  dev-run      Build and run the server locally"
	@echo "  format       Format all C++ source files"
	@echo "  docker-build Build the Docker image"
	@echo "  docker-run   Build and run the Docker image with persistent SQLite storage"

configure:
	conan install . --output-folder=$(BUILD_DIR) --build=missing -s build_type=$(CONFIG)
	cmake -S . -B $(BUILD_DIR) -DCMAKE_TOOLCHAIN_FILE=$(BUILD_DIR)/conan_toolchain.cmake

dev-build:
	cmake --build $(BUILD_DIR) --config $(CONFIG) --target load_balancer

dev-run: dev-build
	cmd /C "call $(BUILD_DIR)\conanrun.bat && $(BUILD_DIR)\$(CONFIG)\load_balancer.exe"

format:
	cmake --build $(BUILD_DIR) --config $(CONFIG) --target format

docker-build:
	docker build -t $(IMAGE) .

docker-run: docker-build
	docker run --rm --name $(CONTAINER) -p $(PORT):3000 -v $(DATA_VOLUME):/data $(IMAGE)
