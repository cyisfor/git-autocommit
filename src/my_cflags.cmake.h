#define MY_CFLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src ${EVENT_CFLAGS}"
#define MY_LDFLAGS "$<TARGET_LINKER_FILE:autocommit> ${EVENT_LDFLAGS}"
#define MY_LDLIBS "$<TARGET_LINKER_FILE:autocommit> ${EVENT_LDLIBS}"
