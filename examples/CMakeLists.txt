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
    NAME continuum-showcase-example
    SOURCE_FILES continuum-showcase-example.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libpoint-to-point}
)

build_lib_example(
    NAME continuum-wifi-showcase-example
    SOURCE_FILES wifi-showcase/continuum-wifi-showcase-example.cc
    LIBRARIES_TO_LINK ${libcontinuum}
                      ${libwifi}
                      ${libspectrum}
                      ${libpropagation}
                      ${libbuildings}
                      ${libmobility}
                      ${libinternet}
                      ${libpoint-to-point}
                      ${libenergy}
)
