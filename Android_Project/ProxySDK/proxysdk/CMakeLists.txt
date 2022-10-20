cmake_minimum_required(VERSION 3.10.2)

#设置生成的so动态库最后输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/libs_export/${ANDROID_ABI})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/libs_export/${ANDROID_ABI})

#设置工程源码根目录
set(Root ${CMAKE_SOURCE_DIR}/../../..)
set(ToolKit_Root ${Root}/../ZLMediaKit/3rdpart/ZLToolKit/src)
set(Json_Root ${Root}/../ZLMediaKit/3rdpart/jsoncpp)
set(Proxy_Root ${Root}/src)

#设置头文件目录
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${Json_Root}/include)
INCLUDE_DIRECTORIES(${Proxy_Root})

add_definitions(-DHAVE_SENDMMSG_API -DHAVE_RECVMMSG_API -DHAVE_MMSG_HDR)

#收集源代码
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)
file(GLOB Json_src_list ${Json_Root}/src/lib_json/*.cpp)
file(GLOB Proxy_src_list ${Proxy_Root}/Proxy/*.cpp ${Proxy_Root}/Proxy/*.h)

#去除win32的适配代码
list(REMOVE_ITEM ToolKit_src_list ${ToolKit_Root}/win32/getopt.c)

#添加库
add_library(jsoncpp STATIC ${Json_src_list})
add_library(zltoolkit STATIC ${ToolKit_src_list})
add_library(proxy STATIC ${Proxy_src_list})
add_library(ProxySDK SHARED src/main/cpp/native-lib.cpp)

#链接
target_link_libraries(ProxySDK proxy jsoncpp zltoolkit log z)




