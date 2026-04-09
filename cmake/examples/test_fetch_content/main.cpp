#include "profiler/log_sink.h"
#include "profiler_manager.h"

int main() {
    profiler::ProfilerManager profiler;
    // Verify the library links correctly — just construct and destroy
    (void)profiler;
    return 0;
}
