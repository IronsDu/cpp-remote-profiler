#include "profiler_manager.h"
#include "profiler/log_sink.h"

int main() {
    profiler::ProfilerManager profiler;
    // Verify the library can be constructed and basic API is accessible
    // Verify the library links correctly — just construct and destroy
    (void)profiler;
    return 0;
}
