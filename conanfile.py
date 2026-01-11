from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.system.package_manager import Apt, Brew


class WordNetConan(ConanFile):
    name = "wordnet"
    version = "3.0"
    license = "WordNet License (MIT-like)"
    author = "Princeton University"
    url = "https://wordnet.princeton.edu"
    description = "WordNet lexical database and command-line tools"
    topics = ("nlp", "lexicon", "wordnet", "linguistics")
    settings = "os", "compiler", "build_type", "arch"
    
    # No external package dependencies required for basic build
    # CMake, Ninja, and compilers are build tools (installed via Conan)
    # Tcl/Tk are optional system dependencies for GUI (wishwn)
    
    exports_sources = "CMakeLists.txt", "src/*", "lib/*", "include/*", "dict/*", "doc/*"
    
    def layout(self):
        cmake_layout(self)
    
    def system_requirements(self):
        """
        Install optional system dependencies for GUI support.
        Tcl/Tk are optional - build will succeed without them, but wishwn (GUI) won't be built.
        """
        # Only install Tcl/Tk if user has configured Conan to install system packages
        # Use -c tools.system.package_manager:mode=install to enable automatic installation
        install_mode = self.conf.get("tools.system.package_manager:mode", default="check") == "install"
        
        if install_mode:
            # Install Tcl/Tk development libraries for GUI support
            apt = Apt(self)
            apt.install(["tcl-dev", "tk-dev"], update=True, check=False)
            
            brew = Brew(self)
            brew.install(["tcl-tk"], check=False)
        
        # Windows typically doesn't need system packages for Tcl/Tk
        # Users can install separately if needed for GUI
    
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
