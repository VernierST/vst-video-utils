from conans import ConanFile, CMake, tools


class VstudmConan(ConanFile):
    python_requires = "vst-conan-helper/0.10@vernierst+vst-native-modules-release/main"
    python_requires_extend = "vst-conan-helper.VstConanBase"

    name = "vstvideoutils"
    description = "Vernier Video Utils"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    generators = "cmake"
    exports_sources = "../*", "!conan/*", "!build/*"
    exports = "../CMakeLists.txt"

    def init(self):
        base = self.python_requires["vst-conan-helper"].module.VstConanBase
        base.vst_setup(base, self)

    def validate(self):
        if self.settings.os != "Emscripten":
            msg = ("%s not supported (only available for Emscripten)" %
                   (self.settings.os))
            raise ConanInvalidConfiguration(msg)

    def set_version(self):
        self.vst_set_version ("CMakeLists.txt")

    def build_requirements(self):
        self.vst_common_build_requirements()
        self.vst_add_build_requirement("openh264", "2.1.0")
        self.vst_add_build_requirement("ffmpeg", "4.2.4")
        self.vst_add_build_requirement("opencv", "4.5.2")

    def build(self):
        cmake = self.vst_cmake()
        cmake.build()

    def package(self):
        self.copy("*", "wasm", "bin")
        self.copy("VideoUtils.js", "wasm", "src")

    def package_id(self):
        self.vst_package_id()

    def build_id(self):
        self.vst_build_id()

    def deploy(self):
        self.copy("*.js")
        self.copy("*.wasm")
