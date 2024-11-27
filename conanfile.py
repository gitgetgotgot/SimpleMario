
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class ExampleRecipe(ConanFile):
    name = "mario"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    def requirements(self):
        self.requires("miniaudio/0.11.21")
        self.requires("freeglut/3.4.0")
        self.requires("stb/cci.20240531")

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
