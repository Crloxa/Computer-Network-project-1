cmake_minimum_required(VERSION 3.16)
project(Project1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 统一输出到 bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_SOURCE_DIR}/bin)

set(SRC_FILES
    src/main.cpp
    src/code.cpp
    src/ffmpeg.cpp
    src/ImgDecode.cpp
    src/pic.cpp
)

# 头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ffmpeg/include
)

# 你的 OpenCV 在 lib 下
set(OPENCV_LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
link_directories(${OPENCV_LIB_DIR})

add_executable(encoder ${SRC_FILES})
target_compile_definitions(encoder PRIVATE BUILD_ENCODER=1)

add_executable(decoder ${SRC_FILES})
target_compile_definitions(decoder PRIVATE BUILD_DECODER=1)

set(TEST_COMMON_SOURCES
    src/code.cpp
    src/ffmpeg.cpp
    src/ImgDecode.cpp
    src/pic.cpp
)

set(WITHOUT_PIC_SRC_FILES
    src/main_without_pic.cpp
    src/code.cpp
    src/ffmpeg.cpp
    src/ImgDecode.cpp
)

add_executable(textcode
    src/textcode.cpp
    ${TEST_COMMON_SOURCES}
)

add_executable(textdecode
    src/textdecode.cpp
    ${TEST_COMMON_SOURCES}
)

add_executable(encoder_without_pic ${WITHOUT_PIC_SRC_FILES})
target_compile_definitions(encoder_without_pic PRIVATE BUILD_ENCODER=1)

add_executable(decoder_without_pic ${WITHOUT_PIC_SRC_FILES})
target_compile_definitions(decoder_without_pic PRIVATE BUILD_DECODER=1)

# 关键：只链接 world 库
target_link_libraries(encoder PRIVATE
    $<$<CONFIG:Debug>:opencv_world4120d.lib>
    $<$<CONFIG:Release>:opencv_world4120.lib>
)

target_link_libraries(decoder PRIVATE
    $<$<CONFIG:Debug>:opencv_world4120d.lib>
    $<$<CONFIG:Release>:opencv_world4120.lib>
)

target_link_libraries(textcode PRIVATE
    $<$<CONFIG:Debug>:opencv_world4120d.lib>
    $<$<CONFIG:Release>:opencv_world4120.lib>
)

target_link_libraries(textdecode PRIVATE
    $<$<CONFIG:Debug>:opencv_world4120d.lib>
    $<$<CONFIG:Release>:opencv_world4120.lib>
)

target_link_libraries(encoder_without_pic PRIVATE
    $<$<CONFIG:Debug>:opencv_world4120d.lib>
    $<$<CONFIG:Release>:opencv_world4120.lib>
)

target_link_libraries(decoder_without_pic PRIVATE
    $<$<CONFIG:Debug>:opencv_world4120d.lib>
    $<$<CONFIG:Release>:opencv_world4120.lib>
)

# 复制依赖到 bin（含 opencv dll）
add_custom_target(copy_deps ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/lib/opencv_world4120.dll ${CMAKE_SOURCE_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/lib/opencv_world4120d.dll ${CMAKE_SOURCE_DIR}/bin/
    COMMENT "Copy OpenCV runtime dlls to bin"
)

add_dependencies(encoder copy_deps)
add_dependencies(decoder copy_deps)
add_dependencies(textcode copy_deps)
add_dependencies(textdecode copy_deps)
add_dependencies(encoder_without_pic copy_deps)
add_dependencies(decoder_without_pic copy_deps)
