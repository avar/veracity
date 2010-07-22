
set(JSON_SOURCES
    sg_ztemplate__branches.json
    sg_ztemplate__comments.json
    sg_ztemplate__stamps.json
    sg_ztemplate__sup.json
    sg_ztemplate__tags.json
    sg_ztemplate__users.json
    sg_ztemplate__audit.json
    sg_ztemplate__wit.json
)

foreach(tj ${JSON_SOURCES})
string(REPLACE ".json" "" basename ${tj})
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${basename}.c
    COMMAND sg_hexdump ${CMAKE_CURRENT_SOURCE_DIR}/${basename}.json ${CMAKE_CURRENT_BINARY_DIR}/${basename}.c
    DEPENDS sg_hexdump ${CMAKE_CURRENT_SOURCE_DIR}/${basename}.json
    )
set(C_SOURCES ${C_SOURCES}
    ${CMAKE_CURRENT_BINARY_DIR}/${basename}.c
)
endforeach(tj ${JSON_SOURCES})

add_library(sgtemplates STATIC ${C_SOURCES})
target_link_libraries(sgtemplates ${SG_OS_LIBS})

add_custom_target(
    template_check
    COMMAND ztcheck
    DEPENDS ztcheck
    )
