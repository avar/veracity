## Build static SGHASH library.  This is a wrapper for all of
## the various hash algorithms that we use (without all of the
## baggage that we don't need).
##################################################################
 
set(C_SOURCES
sha1.c
sha2.c
skein.c skein_block.c
sghash.c
)

file(GLOB HEADERS ./*.h)

set(ALL_SOURCE ${C_SOURCES} ${HEADERS})

add_definitions(-DSHA2_UNROLL_TRANSFORM)

add_library(sghash STATIC ${ALL_SOURCE})

##################################################################
## A little throw-away test program that computes hashes for known
## inputs and verifies that we get the right answer.

add_executable(sghash_test sghash_test.c)
target_link_libraries(sghash_test sghash ${SG_OK_LIBS})
add_test(sghash_test_sha1_160     ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SHA1/160")
add_test(sghash_test_sha2_256     ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SHA2/256")
add_test(sghash_test_sha2_384     ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SHA2/384")
add_test(sghash_test_sha2_512     ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SHA2/512")
add_test(sghash_test_skein_256    ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SKEIN/256")
add_test(sghash_test_skein_512    ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SKEIN/512")
add_test(sghash_test_skein_1024   ${EXECUTABLE_OUTPUT_PATH}/sghash_test "SKEIN/1024")
