idf_build_get_property(target IDF_TARGET)

idf_component_register(INCLUDE_DIRS include)

set(lib "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/libesp-code-scanner.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libnewlib_iconv.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib})
