set(C_SOURCES sg_mongoose.c sg_mongoose.h)
set(ALL_SOURCE ${C_SOURCES})
add_library(sgmongoose STATIC ${ALL_SOURCE})

IF (WIN32)
target_link_libraries(sgmongoose wsock32.lib)
ELSE (WIN32)
target_link_libraries(sgmongoose)
ENDIF (WIN32)
