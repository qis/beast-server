cmake_minimum_required(VERSION 3.15.3 FATAL_ERROR)
project(server DESCRIPTION "Server" VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  add_definitions(-D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-D_ATL_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)
else()
  set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/res/cmake)

set(PROJECT_VENDOR "Company")
configure_file(res/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/version.h LF)

file(GLOB_RECURSE headers CONFIGURE_DEPENDS src/*.hpp)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp src/*.manifest src/*.rc)

add_executable(${PROJECT_NAME} ${headers} ${sources})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src src)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

find_package(Boost 1.71 REQUIRED system)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system)

target_sources(${PROJECT_NAME} PRIVATE src/asio.cpp src/beast.cpp)
target_compile_definitions(${PROJECT_NAME} PUBLIC
  BOOST_ASIO_HAS_CO_AWAIT=1
  BOOST_ASIO_SEPARATE_COMPILATION=1
  BOOST_BEAST_SEPARATE_COMPILATION=1
  BOOST_BEAST_USE_STD_STRING_VIEW=1)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE crypt32 ws2_32)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto OpenSSL::SSL)

if(WIN32)
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
  file(GLOB THIRD_PARTY_LIBRARIES CONFIGURE_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/*.dll")
  install(FILES ${THIRD_PARTY_LIBRARIES} DESTINATION .)
  install(FILES res/license.rtf DESTINATION .)
else()
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
  install(FILES res/license.txt DESTINATION share)
  install(FILES
    /opt/llvm/lib/libc++.so
    /opt/llvm/lib/libc++.so.1
    /opt/llvm/lib/libc++.so.1.0
    /opt/llvm/lib/libc++abi.so
    /opt/llvm/lib/libc++abi.so.1
    /opt/llvm/lib/libc++abi.so.1.0
    /opt/llvm/lib/libgomp.so
    /opt/llvm/lib/libiomp5.so
    /opt/llvm/lib/libomp.so
    /opt/llvm/lib/libomptarget.rtl.x86_64.so
    /opt/llvm/lib/libomptarget.so
    /opt/llvm/lib/libtbb.so
    /opt/llvm/lib/libtbb.so.2
    /opt/llvm/lib/libtbbmalloc.so
    /opt/llvm/lib/libtbbmalloc.so.2
    /opt/llvm/lib/libtbbmalloc_proxy.so
    /opt/llvm/lib/libtbbmalloc_proxy.so.2
    /opt/llvm/lib/libunwind.so
    /opt/llvm/lib/libunwind.so.1
    /opt/llvm/lib/libunwind.so.1.0
    DESTINATION lib)
endif()

set(CPACK_PACKAGE_VENDOR "${PROJECT_VENDOR}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_DESCRIPTION}")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")
if(WIN32)
  set(CPACK_GENERATOR "NSIS64")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_DESCRIPTION}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/res/license.rtf")
  set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src\\\\main.ico")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "${PROJECT_NAME}.exe")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)
else()
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${PROJECT_NAME}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/res/license.txt")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.27)")
  set(CPACK_DEBIAN_PACKAGE_SECTION "misc")
endif()
include(CPack)
