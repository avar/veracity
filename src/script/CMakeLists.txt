set(C_SOURCES vscript vscript.c )

file(GLOB HEADERS ../libraries/include/*.h)
set(ALL_SOURCE ${C_SOURCES} ${HEADERS})

add_executable(${ALL_SOURCE})

target_link_libraries(vscript sglib)

add_dependencies(vscript template_check)

