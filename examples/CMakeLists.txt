build_lib_example(
    NAME continuum-example
    SOURCE_FILES continuum-example.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-multi-client-example
    SOURCE_FILES continuum-multi-client-example.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)
