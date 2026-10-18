import os

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class MagicFormulaConan(ConanFile):
    """Consumer recipe: dependencies and CMake integration files for magic-formula."""

    settings = "os", "arch", "compiler", "build_type"

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")

    def layout(self):
        # build/conan/<build_type>/conan_toolchain.cmake, referenced by CMakePresets.json
        self.folders.generators = os.path.join("build", "conan", str(self.settings.build_type).lower())

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False  # CMakePresets.json is maintained by hand
        toolchain.generate()
