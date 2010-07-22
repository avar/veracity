set(C_SOURCES vv
sg.c
sg_logfile.c
sg__do_cmd_merge.c
sg__do_cmd_resolve.c
sg__do_cmd_update.c
)

file(GLOB HEADERS ../libraries/include/*.h)
file(GLOB TEMPLATES templates/*.xhtml)
file(GLOB STATICS static/*.gif static/*.js static/*.css static/*.png static/*.jpg)

set(ALL_SOURCE ${C_SOURCES} ${HEADERS} ${TEMPLATES} ${STATICS} ../libraries/mongoose/sg_mongoose.h)

source_group("Static Web Content" FILES ${STATICS})
source_group("XHTML Templates" FILES ${TEMPLATES})

add_executable(${ALL_SOURCE})

target_link_libraries(vv sglib sgmongoose)

add_dependencies(vv template_check)
