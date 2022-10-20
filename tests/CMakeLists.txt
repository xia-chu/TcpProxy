include_directories(./)

#########################################################################
set(SRC_LIST TcpProxyClient.cpp)
add_executable(TcpProxyClient ${SRC_LIST})
target_link_libraries(TcpProxyClient  proxy json zltoolkit ${LINK_LIB_LIST})

#########################################################################
set(SRC_LIST TcpProxyBenchmark.cpp)
add_executable(TcpProxyBenchmark ${SRC_LIST})
target_link_libraries(TcpProxyBenchmark proxy json zltoolkit ${LINK_LIB_LIST})

#########################################################################
set(SRC_LIST TcpProxyServer.cpp)
add_executable(TcpProxyServer ${SRC_LIST})
target_link_libraries(TcpProxyServer proxy json zltoolkit ${LINK_LIB_LIST} )
