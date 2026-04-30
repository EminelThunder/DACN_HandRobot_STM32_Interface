cmake_minimum_required(VERSION 3.19)
project(RVM1_Interface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain (eigen3, assimp)
# Nếu chưa set, truyền khi configure:
#   -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" AND
   NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

# Đường dẫn Qt 6 (MinGW 64-bit)
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialPort 3DCore 3DRender 3DExtras)
find_package(Eigen3 REQUIRED)

qt_standard_project_setup()

qt_add_executable(RVM1_Interface
    WIN32
    main.cpp
    # UI
    ui/mainwindow.cpp
    ui/mainwindow.h
    ui/mainwindow.ui
    ui/RobotRenderer.cpp
    ui/RobotRenderer.h
    # Core
    core/Kinematics.cpp
    core/Kinematics.h
    core/SerialManager.cpp
    core/SerialManager.h
)

target_include_directories(RVM1_Interface PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(RVM1_Interface
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::SerialPort
        Qt6::3DCore
        Qt6::3DRender
        Qt6::3DExtras
        Eigen3::Eigen
)

# Copy assets vào thư mục build
add_custom_command(TARGET RVM1_Interface POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:RVM1_Interface>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/../3D/STL_parts"
        "$<TARGET_FILE_DIR:RVM1_Interface>/assets/parts"
)
