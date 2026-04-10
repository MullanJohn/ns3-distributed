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

build_lib_example(
    NAME continuum-periodic-evaluation
    SOURCE_FILES continuum-periodic-evaluation-homog.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
                      ${libwifi}
                      ${libspectrum}
                      ${libmobility}
)


build_lib_example(
    NAME continuum-periodic-evaluation-hetero
    SOURCE_FILES continuum-periodic-evaluation-hetero.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
                      ${libwifi}
                      ${libspectrum}
                      ${libmobility}
)

build_lib_example(
    NAME continuum-periodic-evaluation-5g
    SOURCE_FILES continuum-periodic-evaluation-5g.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
                      ${liblte}
                      ${libmobility}
)

build_lib_example(
    NAME continuum-single-metric-validation
    SOURCE_FILES ../eval/single-metric/single-metric-validation.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-system-invariants-validation
    SOURCE_FILES ../eval/system-invariants/system-invariants-validation.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-dag-validation
    SOURCE_FILES ../eval/dag-coverage/dag-validation.cc
                 ../eval/dag-coverage/dag-validation-client.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-period-wired
    SOURCE_FILES continuum-period-wired-homog.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libcsma}
)

build_lib_example(
    NAME continuum-period-wired-hetero
    SOURCE_FILES continuum-period-wired-hetero.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libcsma}
)
