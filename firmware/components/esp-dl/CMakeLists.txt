idf_build_get_property(target IDF_TARGET)

idf_component_register(
    INCLUDE_DIRS
        include/
        include/tool
        include/typedef
        include/image
        include/math
        include/nn
        include/tvm
        include/layer
        include/detect
        include/model_zoo
)

set(lib "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/libcat_face_detect.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/libhuman_face_detect.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/libcolor_detect.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/libmfn.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/${target}/libdl.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib})
