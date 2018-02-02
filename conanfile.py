from conans import ConanFile, CMake


class InexorgluegenConan(ConanFile):
    name = "InexorGlueGen"
    version = "0.6.5"
    description = """This is the Conan package for the Inexor game gluecode generator, which generates our network code (which is also our scripting binding)
                     to sync variables/classes/lists without writing extra code."""
    license = "ZLIB"
    url = "https://github.com/inexorgame/inexor-core/"
    # Note:  we always want it to be built as release build, as its distributed as executable only.
    settings = "os", "compiler", "build_type", "arch"
    requires = (("Kainjow_Mustache/2.0@inexorgame/stable"),
            ("pugixml/1.7@inexorgame/stable"),
            ("Boost/1.66.0@conan/stable"))

    # Usage dependencies: grpc (+ protobuf), doxygen
    generators = "cmake"
    exports_sources = "inexor*", "cmake*", "CMakeLists.txt", "require_run_gluegen.cmake"

    def build(self):
        cmake = CMake(self)
        args = [""]
        self.run('cmake . {} {}'.format(cmake.command_line, " ".join(args)))
        self.run("cmake --build . {}".format(cmake.build_config))

    def package(self):
        self.copy("*gluecodegenerato*", dst="bin", src="bin", keep_path=False)
        self.copy("*.dll", dst="bin", src="lib", keep_path=False)
        self.copy("*.so", dst="lib", src="lib", keep_path=False)
        self.copy("require_run_gluegen.cmake", dst="", src="", keep_path=False)

    def package_info(self):
        self.cpp_info.bindirs = ["bin"]
