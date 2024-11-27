CMake and conan package manager must be present. Both must be in PATH 
 
How to build: 
1. $ conan profile detect 
2. $ conan install . --build=missing 
3. $ cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_TOOLCHAIN_FILE="generators/conan_toolchain.cmake" -S ./ -B ./build 
4. $ cmake --build ./build --parallel 12 --config Release 
5. Executable will be /build/Release/mario 