import os

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class MagicFormulaConan(ConanFile):
    """Consumer recipe: dependencies and CMake integration files for magic-formula."""

    license = "Apache-2.0"
    settings = "os", "arch", "compiler", "build_type"

    # "auto" = Google Benchmark for Release only; True/False force it on/off for any build type.
    # with_tests=False drops every test requirement (gtest and benchmark), e.g. for the WebAssembly host profile.
    options = {"with_benchmark": ["auto", True, False], "with_tests": [True, False]}
    default_options = {"with_benchmark": "auto", "with_tests": True}

    def requirements(self):
        self.requires("eigen/5.0.1")

    def build_requirements(self):
        if not self.options.with_tests:
            return
        self.test_requires("gtest/1.17.0")
        want = str(self.options.with_benchmark)
        if want == "True" or (want == "auto" and str(self.settings.build_type) == "Release"):
            self.test_requires("benchmark/1.9.5")

    def layout(self):
        # build/conan/<build_type>/conan_toolchain.cmake (build/conan/wasm for the Emscripten host), referenced by
        # CMakePresets.json
        if str(self.settings.os) == "Emscripten":
            self.folders.generators = os.path.join("build", "conan", "wasm")
        else:
            self.folders.generators = os.path.join("build", "conan", str(self.settings.build_type).lower())

    def generate(self):
        CMakeDeps(self).generate()
        # CMakePresets.json hard-codes Ninja for every OS. Without this Conan defaults to the Visual Studio generator
        # for msvc and writes CMAKE_GENERATOR_PLATFORM/TOOLSET, which Ninja rejects. No effect on Linux and macOS.
        toolchain = CMakeToolchain(self, generator="Ninja")
        toolchain.user_presets_path = False  # CMakePresets.json is maintained by hand
        toolchain.generate()
