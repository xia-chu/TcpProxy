cmake_minimum_required(VERSION 3.1)
project(TcpProxy)

#使能c++11
#add_compile_options(-std=c++11)
set(CMAKE_CXX_STANDARD 14)

#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")
#设置库文件路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
#设置可执行程序路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

link_directories(${LIBRARY_OUTPUT_PATH})

#设置工程源码根目录
set(ToolKit_Root ${CMAKE_SOURCE_DIR}/../ZLMediaKit/3rdpart/ZLToolKit/src)
set(Json_Root ${CMAKE_SOURCE_DIR}/../ZLMediaKit/3rdpart/jsoncpp)
set(Proxy_Root ${CMAKE_SOURCE_DIR}/src/Proxy)

#设置头文件目录
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/)
INCLUDE_DIRECTORIES(${Json_Root}/include/)

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
file(GLOB Json_src_list ${Json_Root}/src/lib_json/*.cpp)
file(GLOB Proxy_src_list ${Proxy_Root}/*.cpp ${Proxy_Root}/*.h ${Proxy_Root}/*.c)

#去除win32的适配代码
if (NOT WIN32)
    list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)
endif ()

#添加库
add_library(zltoolkit STATIC ${ToolKit_src_list})
add_library(json STATIC ${Json_src_list})
add_library(proxy STATIC ${Proxy_src_list})

#设置需要链接的库
set(LINK_LIB_LIST pthread)

option(ENABLE_OPENSSL "Enable openssl" off)

if (ENABLE_OPENSSL)
    #查找openssl是否安装
    find_package(OpenSSL QUIET)
    if (OPENSSL_FOUND)
        message(STATUS "找到openssl库:\"${OPENSSL_LIBRARIES}\",ENABLE_OPENSSL宏已打开")
        include_directories(${OPENSSL_INCLUDE_DIR})
        add_definitions(-DENABLE_OPENSSL)
        list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
    else ()
        message(FATAL_ERROR "未安装OpenSSL")
    endif ()
else ()
    message(WARNING "openssl未打开")
endif ()

add_subdirectory(tests)