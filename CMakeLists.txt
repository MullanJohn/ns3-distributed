check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/distributed-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME distributed
    SOURCE_FILES model/distributed.cc
                 helper/distributed-helper.cc
    HEADER_FILES model/distributed.h
                 helper/distributed-helper.h
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES test/distributed-test-suite.cc
                 ${examples_as_tests_sources}
)
