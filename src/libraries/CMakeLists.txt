
add_subdirectory(mongoose)
add_subdirectory(sghash)
add_subdirectory(ut)
add_subdirectory(templates)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/doxygen.conf DC)
string(REPLACE "SG__OUTPUT_DIRECTORY__SG" "${CMAKE_BINARY_DIR}/doxygen" DC2 "${DC}")
string(REPLACE "SG__INPUT__SG" "${CMAKE_CURRENT_SOURCE_DIR}/include" DC3 "${DC2}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/doxygen.conf "${DC3}")

add_custom_target(
    doxygen
    COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/doxygen.conf
    )

