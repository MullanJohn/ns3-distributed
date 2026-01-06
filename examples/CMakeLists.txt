build_lib_example(
    NAME distributed-example
    SOURCE_FILES distributed-example.cc
    LIBRARIES_TO_LINK ${libdistributed}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME distributed-multi-client-example
    SOURCE_FILES distributed-multi-client-example.cc
    LIBRARIES_TO_LINK ${libdistributed}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME load-balancer-example
    SOURCE_FILES load-balancer-example.cc
    LIBRARIES_TO_LINK ${libdistributed}
                      ${libpoint-to-point}
)
