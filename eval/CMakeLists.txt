build_lib_example(
    NAME continuum-dag-validation
    SOURCE_FILES dag-coverage/dag-validation.cc
                 dag-coverage/dag-validation-client.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-single-metric-validation
    SOURCE_FILES single-metric/single-metric-validation.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-system-invariants-validation
    SOURCE_FILES system-invariants/system-invariants-validation.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)
