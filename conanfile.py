from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import get, copy
import os

required_conan_version = ">=1.53.0"

class CppRemoteProfilerConan(ConanFile):
    name = "cpp-remote-profiler"
    version = "0.1.0"
    description = "C++ Remote Profiler - A performance profiling library similar to Go pprof"
    license = "MIT"
    author = "Your Name <your.email@example.com>"
    url = "https://github.com/your-org/cpp-remote-profiler"
    homepage = "https://github.com/your-org/cpp-remote-profiler"
    topics = ("profiler", "performance", "pprof", "gperftools", "cpp20")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_web": [True, False],
        "with_symbolize": [True, False]
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "with_web": True,
        "with_symbolize": True
    }

    # Export headers
    exports_sources = "include/*", "src/*", "CMakeLists.txt"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("gperftools/2.15")
        self.requires("nlohmann_json/3.11.2")
        self.requires("backward-cpp/1.6")
        self.requires("abseil/20230125.3")

        if self.options.with_web:
            self.requires("drogon/1.9.0", transitive_headers=True, transitive_libs=True)

        if self.options.with_symbolize:
            self.requires("backward-cpp/1.6")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["REMOTE_PROFILER_BUILD_EXAMPLES"] = False
        tc.variables["REMOTE_PROFILER_BUILD_TESTS"] = False
        tc.variables["REMOTE_PROFILER_INSTALL"] = True
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "*.h", dst=os.path.join(self.package_folder, "include"), src=self.source_folder + "/include")
        copy(self, "*.a", dst=os.path.join(self.package_folder, "lib"), src=self.build_folder + "/lib", keep_path=False)
        copy(self, "*.so", dst=os.path.join(self.package_folder, "lib"), src=self.build_folder + "/lib", keep_path=False)
        copy(self, "*.dylib", dst=os.path.join(self.package_folder, "lib"), src=self.build_folder + "/lib", keep_path=False)
        copy(self, "*.dll", dst=os.path.join(self.package_folder, "bin"), src=self.build_folder + "/bin", keep_path=False)
        copy(self, "*.lib", dst=os.path.join(self.package_folder, "lib"), src=self.build_folder + "/lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["profiler_lib"]

        if self.options.with_web:
            self.cpp_info.defines.append("REMOTE_PROFILER_ENABLE_WEB=1")

        if self.options.with_symbolize:
            self.cpp_info.defines.append("REMOTE_PROFILER_ENABLE_SYMBOLIZE=1")

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["pthread", "dl"])

    def validate(self):
        if self.settings.os == "Windows" and self.options.with_web:
            self.output.warning("Web support on Windows is experimental")

        if self.settings.compiler.cppstd:
            check_min_cppstd(self, "20")
