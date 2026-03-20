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

# 头文件：使用绝对路径，确保编译器能找到 OpenCV 和 ffmpeg 头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/ffmpeg/include
)

# OpenCV 库绝对路径，避免 link_directories + 短名称在 MSVC 下不可靠的问题
set(OPENCV_LIB_DEBUG   ${CMAKE_SOURCE_DIR}/lib/opencv_world4120d.lib)
set(OPENCV_LIB_RELEASE ${CMAKE_SOURCE_DIR}/lib/opencv_world4120.lib)
set(OPENCV_LIBS
    $<$<CONFIG:Debug>:${OPENCV_LIB_DEBUG}>
    $<$<CONFIG:Release>:${OPENCV_LIB_RELEASE}>
    $<$<CONFIG:RelWithDebInfo>:${OPENCV_LIB_RELEASE}>
    $<$<CONFIG:MinSizeRel>:${OPENCV_LIB_RELEASE}>
)

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

# 链接 OpenCV world 库（全部 target 统一处理）
foreach(target encoder decoder textcode textdecode encoder_without_pic decoder_without_pic)
    target_link_libraries(${target} PRIVATE ${OPENCV_LIBS})
endforeach()

# 复制依赖到 bin（含 opencv dll）
add_custom_target(copy_deps ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/lib/opencv_world4120.dll ${CMAKE_SOURCE_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/lib/opencv_world4120d.dll ${CMAKE_SOURCE_DIR}/bin/
    COMMENT "Copy OpenCV runtime dlls to bin"
)

foreach(target encoder decoder textcode textdecode encoder_without_pic decoder_without_pic)
    add_dependencies(${target} copy_deps)
endforeach()
