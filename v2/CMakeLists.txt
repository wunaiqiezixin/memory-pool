cmake_minimum_required (VERSION 2.8)

project (v2)

aux_source_directory (src SRC_LIST)

aux_source_directory (tests TESTS)

add_executable (test.out ${TESTS} ${SRC_LIST})



