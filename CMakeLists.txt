cmake_minimum_required(VERSION 3.23)
project(mario)

find_package(GLUT REQUIRED)
find_package(miniaudio REQUIRED)
find_package(stb REQUIRED)

SET(SOURCES ./src/main.cpp ./src/textures.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
file(COPY media DESTINATION ${CMAKE_BINARY_DIR}/)

target_link_libraries(${PROJECT_NAME} GLUT::GLUT glu::glu opengl::opengl miniaudio::miniaudio stb::stb)