#pragma once

#include <os/signpost.h>

// os_signpost calls add per-event overhead even when nothing is tracing.
// build with -DSMHKD_SIGNPOSTS to enable them for profiling.
#ifdef SMHKD_SIGNPOSTS
#define SIGNPOST_GENERATE(log) os_signpost_id_generate(log)
#define SIGNPOST_BEGIN(...) os_signpost_interval_begin(__VA_ARGS__)
#define SIGNPOST_END(...) os_signpost_interval_end(__VA_ARGS__)
#else
#define SIGNPOST_GENERATE(log) ((void)(log), OS_SIGNPOST_ID_NULL)
#define SIGNPOST_BEGIN(...) ((void)0)
#define SIGNPOST_END(...) ((void)0)
#endif
