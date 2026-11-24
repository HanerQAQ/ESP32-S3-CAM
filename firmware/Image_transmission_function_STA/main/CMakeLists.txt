set(src_dirs
        .
        src
)

set(include_dirs
        include
        .
)

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    EMBED_FILES
        "../frontend/gzipped/index.html.gz"
        "../frontend/gzipped/index_ov2640.html.gz"
        "../frontend/gzipped/index_gc2145.html.gz"
        "../frontend/gzipped/monitor.html.gz"
)

set_source_files_properties(src/app_myhttpd.cpp PROPERTIES COMPILE_OPTIONS "-Wno-missing-field-initializers")
