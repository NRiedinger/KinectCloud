cmake_minimum_required(VERSION 3.0...3.25)
project(
    "C++ WebGPU Test" # name of the project, which will also be the name of the visual studio solution if you use it
    VERSION 0.1.0 # any version number
    LANGUAGES CXX C # programming languages used by the project
)

# Include glfw directory
add_subdirectory(libs/glfw)
# Include webgpu directory, to define the 'webgpu' target
add_subdirectory(libs/webgpu)

#add_subdirectory(libs/glfw3webgpu)


# create a sources variable with a link to all cpp files to compile
set(SOURCES
	src/main.cpp
	src/Application.cpp
	src/Application.h
	src/glfw3webgpu.c
	src/glfw3webgpu.h
	src/implementations.cpp
	src/ResourceManager.cpp
	src/ResourceManager.h
)
add_executable(App ${SOURCES})

# add libraries as dependency to App
set(LIBRARIES
	webgpu
	glfw
)
target_link_libraries(App PRIVATE ${LIBRARIES})


option(DEV_MODE "set up development helper settings" ON)
if(DEV_MODE)
	target_compile_definitions(App PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
else()
	target_compile_definitions(App PRIVATE RESOURCE_DIR="./resources")
endif()


# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary.
target_copy_webgpu_binaries(App)


set_target_properties(App PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR OFF
)

if (MSVC)
    target_compile_options(App PRIVATE /W4)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()
