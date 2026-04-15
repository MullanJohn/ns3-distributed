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

build_lib_example(
    NAME continuum-wireless-evaluation-homog
    SOURCE_FILES evaluation/wireless-evaluation-homog.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
                      ${libwifi}
                      ${libspectrum}
                      ${libmobility}
)

build_lib_example(
    NAME continuum-wireless-evaluation-hetero
    SOURCE_FILES evaluation/wireless-evaluation-hetero.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
                      ${libwifi}
                      ${libspectrum}
                      ${libmobility}
)

build_lib_example(
    NAME continuum-wireless-evaluation-mu
    SOURCE_FILES evaluation/wireless-evaluation-mu.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
                      ${libwifi}
                      ${libspectrum}
                      ${libmobility}
)
