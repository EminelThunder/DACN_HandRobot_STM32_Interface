cmake_minimum_required(VERSION 3.19)
project(RVM1_Interface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg toolchain (eigen3)
# Tự tìm vcpkg trong các vị trí phổ biến; override bằng -DCMAKE_TOOLCHAIN_FILE=...
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    foreach(_VCPKG_ROOT
            "C:/vcpkg"
            "D:/vcpkg"
            "$ENV{USERPROFILE}/vcpkg"
            "$ENV{LOCALAPPDATA}/vcpkg")
        if(EXISTS "${_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
            set(CMAKE_TOOLCHAIN_FILE "${_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
            message(STATUS "Auto-detected vcpkg at: ${_VCPKG_ROOT}")
            break()
        endif()
    endforeach()
endif()

# Đường dẫn Qt 6 — tự tìm trong các ổ đĩa/phiên bản phổ biến
# Có thể override bằng: -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/mingw_64
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(_QT_ARCHS "mingw_64" "mingw64")
    set(_QT_SEARCH_ROOTS "C:/Qt" "D:/Qt" "E:/Qt" "$ENV{USERPROFILE}/Qt")
    foreach(_ROOT ${_QT_SEARCH_ROOTS})
        if(EXISTS "${_ROOT}")
            file(GLOB _QT_VERSIONS "${_ROOT}/6.*")
            # Sắp xếp giảm dần để ưu tiên phiên bản mới nhất
            list(SORT _QT_VERSIONS ORDER DESCENDING)
            foreach(_VER ${_QT_VERSIONS})
                foreach(_ARCH ${_QT_ARCHS})
                    if(EXISTS "${_VER}/${_ARCH}/lib/cmake/Qt6")
                        set(CMAKE_PREFIX_PATH "${_VER}/${_ARCH}")
                        message(STATUS "Auto-detected Qt6 at: ${CMAKE_PREFIX_PATH}")
                        break()
                    endif()
                endforeach()
                if(DEFINED CMAKE_PREFIX_PATH)
                    break()
                endif()
            endforeach()
        endif()
        if(DEFINED CMAKE_PREFIX_PATH)
            break()
        endif()
    endforeach()
    if(NOT DEFINED CMAKE_PREFIX_PATH)
        message(WARNING "Qt6 không tìm thấy tự động. Truyền thủ công:\n"
                        "  cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/mingw_64 ..")
    endif()
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
