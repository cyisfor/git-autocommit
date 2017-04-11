#include <uv.h>

void check_accept(uv_stream_t* server);
void check_init(void);
typedef struct check_context *CC;
#include <stdint.h>
typedef uint16_t u16;
void check_path(CC ctx, char* path, u16 len);
