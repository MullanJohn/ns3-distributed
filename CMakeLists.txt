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
                 model/task.cc
                 model/gpu-accelerator.cc
                 model/offload-header.cc
                 model/offload-client.cc
                 model/offload-server.cc
                 model/cluster.cc
                 model/node-scheduler.cc
                 model/round-robin-scheduler.cc
                 helper/distributed-helper.cc
                 helper/offload-client-helper.cc
                 helper/offload-server-helper.cc
    HEADER_FILES model/distributed.h
                 model/task.h
                 model/gpu-accelerator.h
                 model/offload-header.h
                 model/offload-client.h
                 model/offload-server.h
                 model/cluster.h
                 model/node-scheduler.h
                 model/round-robin-scheduler.h
                 helper/distributed-helper.h
                 helper/offload-client-helper.h
                 helper/offload-server-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libnetwork}
                      ${libinternet}
                      ${libpoint-to-point}
    TEST_SOURCES test/distributed-test-suite.cc
                 test/gpu-accelerator-test.cc
                 test/gpu-accelerator-queue-test.cc
                 test/offload-header-test.cc
                 test/offload-server-test.cc
                 test/offload-client-test.cc
                 test/cluster-test.cc
                 test/node-scheduler-test.cc
                 ${examples_as_tests_sources}
)
