cmake_minimum_required(VERSION 3.0...3.25)
project(
    "KinectCloud" # name of the project, which will also be the name of the visual studio solution if you use it
    VERSION 1.0.0 # any version number
    LANGUAGES CXX C # programming languages used by the project
)

# IMPORTANT TO SET PATHS
# set path to your vcpkg.cmake file (e.g. "C:/vcpkg/scripts/buildsystems/vcpkg.cmake)")
include(C:/vcpkg/scripts/buildsystems/vcpkg.cmake)

# set path of your Azure Kinect SDK installation (e.g. "C:/Program Files/Azure Kinect SDK v1.4.2")
set(KINECT_SDK_PATH "C:/Program Files/Azure Kinect SDK v1.4.2")


# Include glfw directory
add_subdirectory(libs/glfw)
# Include webgpu directory, to define the 'webgpu' target
add_subdirectory(libs/webgpu)
# include ImGUI
add_subdirectory(libs/imgui)


# create a sources variable with a link to all cpp files to compile
set(SOURCES
	src/main.cpp
	
	src/Application.cpp
	src/Application.h
	
	src/ResourceManager.cpp
	src/ResourceManager.h
	
	src/Camera.h
	src/Camera.cpp
	
	src/Texture.h
	src/Texture.cpp
	
	src/CameraCaptureSequence.h
	src/CameraCaptureSequence.cpp
	
	src/Helpers.h
	src/Helpers.cpp
	
	src/PointcloudRenderer.h
	src/PointcloudRenderer.cpp
	
	src/Pointcloud.h
	src/Pointcloud.cpp
	
	src/K4ADeviceSelector.cpp
	src/K4ADeviceSelector.h
	
	src/Darkmode.h
	src/Darkmode.cpp
	
	src/Structs.h
	
	
	# utils
	src/utils/implementations.cpp
	src/utils/tinyply.h
	src/utils/glfw3webgpu.c
	src/utils/glfw3webgpu.h
	src/utils/stb_image_write.h
	src/utils/imfilebrowser.h
	
	src/utils/k4aimguiextensions.h
	src/utils/k4aimguiextensions.cpp
)
add_executable(KinectCloud ${SOURCES})

# for glm
target_include_directories(KinectCloud PRIVATE ./libs)
target_compile_definitions(KinectCloud PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(KinectCloud PRIVATE GLM_FORCE_LEFT_HANDED) 

# Azure Kinect SDK
target_include_directories(KinectCloud PRIVATE "${KINECT_SDK_PATH}/sdk/include")
target_link_directories(KinectCloud PRIVATE "${KINECT_SDK_PATH}/sdk/windows-desktop/amd64/release/lib")
set(AZURE_KINECT_DLL "${KINECT_SDK_PATH}/sdk/windows-desktop/amd64/release/bin/k4a.dll")
set(DEPTHENGINE_DLL "${KINECT_SDK_PATH}/sdk/windows-desktop/amd64/release/bin/depthengine_2_0.dll")
add_custom_command(TARGET KinectCloud POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${AZURE_KINECT_DLL}"
    "$<TARGET_FILE_DIR:KinectCloud>"
)
add_custom_command(TARGET KinectCloud POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${DEPTHENGINE_DLL}"
    "$<TARGET_FILE_DIR:KinectCloud>"
)


# PCL
find_package(PCL REQUIRED COMPONENTS io common registration)

# add libraries as dependency to App
set(LIBRARIES
	webgpu
	glfw
	imgui
	k4a
	${PCL_LIBRARIES}
)
target_link_libraries(KinectCloud PRIVATE ${LIBRARIES})


option(DEV_MODE "set up development helper settings" ON)
if(DEV_MODE)
	target_compile_definitions(KinectCloud PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
	target_compile_definitions(KinectCloud PRIVATE EXPORT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/export")
	target_compile_definitions(KinectCloud PRIVATE TOOLS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tools")
	target_compile_definitions(KinectCloud PRIVATE CAPTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/captures")
	target_compile_definitions(KinectCloud PRIVATE TMP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tmp")
else()
	target_compile_definitions(KinectCloud PRIVATE RESOURCE_DIR="./resources")
	target_compile_definitions(KinectCloud PRIVATE EXPORT_DIR="./export")
	target_compile_definitions(KinectCloud PRIVATE TOOLS_DIR="./tools")
	target_compile_definitions(KinectCloud PRIVATE CAPTURE_DIR="./captures")
	target_compile_definitions(KinectCloud PRIVATE TMP_DIR="./tmp")
endif()


# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary.
target_copy_webgpu_binaries(KinectCloud)


set_target_properties(KinectCloud PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR OFF
)

if (MSVC)
    target_compile_options(KinectCloud PRIVATE /W4)
else()
    target_compile_options(KinectCloud PRIVATE -Wall -Wextra -pedantic)
endif()
