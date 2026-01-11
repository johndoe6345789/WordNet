from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout


class WordNetConan(ConanFile):
    name = "wordnet"
    version = "3.0"
    license = "WordNet License (MIT-like)"
    author = "Princeton University"
    url = "https://wordnet.princeton.edu"
    description = "WordNet lexical database and command-line tools"
    topics = ("nlp", "lexicon", "wordnet", "linguistics")
    settings = "os", "compiler", "build_type", "arch"
    
    # No dependencies required for basic build
    # Tcl/Tk are optional system dependencies
    
    exports_sources = "CMakeLists.txt", "src/*", "lib/*", "include/*", "dict/*", "doc/*"
    
    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
    
    def package_info(self):
        self.cpp_info.libs = ["WN"]
        self.cpp_info.includedirs = ["include"]
