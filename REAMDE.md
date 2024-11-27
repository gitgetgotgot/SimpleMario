CMake and conan package manager must be present. Both must be in PATH 
 
How to build: 
1. $ conan profile detect 
2. $ conan install . --build=missing 
3. $ cmake --preset conan-default  
4. $ cmake --build ./build --preset conan-release  
5. Executable will be /build/Release/mario 