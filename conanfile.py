from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class LoadBalancer(ConanFile):
    name = "load-balancer"
    version = "0.1"

    settings = "os", "arch", "compiler", "build_type"

    requires = (
        "boost/1.89.0",
        "spdlog/1.15.3",
        "nlohmann_json/3.12.0",
    )

    test_requires = (
        "gtest/1.16.0",
    )

    generators = (
        CMakeDeps,
        CMakeToolchain,
    )
