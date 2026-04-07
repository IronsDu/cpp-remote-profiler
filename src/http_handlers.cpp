/// @file http_handlers.cpp
/// @brief Framework-agnostic HTTP endpoint handlers implementation

#include "profiler/http_handlers.h"
#include "profiler_manager.h"
#include <chrono>
#include <fstream>
#include <sstream>

PROFILER_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static HandlerResponse errorResp(int status, const std::string& message) {
    return HandlerResponse::error(status, message);
}

static bool validateOutputType(const std::string& output_type) {
    return output_type == "flamegraph" || output_type == "pprof";
}

static int clampDuration(int duration, int lo, int hi) {
    if (duration < lo) return lo;
    if (duration > hi) return hi;
    return duration;
}

// ---------------------------------------------------------------------------
// ProfilerHttpHandlers
// ---------------------------------------------------------------------------

ProfilerHttpHandlers::ProfilerHttpHandlers(ProfilerManager& profiler) : profiler_(profiler) {}

// --- Status ---

HandlerResponse ProfilerHttpHandlers::handleStatus() {
    auto cpu = profiler_.getProfilerState(ProfilerType::CPU);
    auto heap = profiler_.getProfilerState(ProfilerType::HEAP);
    auto growth = profiler_.getProfilerState(ProfilerType::HEAP_GROWTH);

    std::ostringstream json;
    json << "{";
    json << "\"cpu\":{\"running\":" << (cpu.is_running ? "true" : "false")
         << ",\"output_path\":\"" << cpu.output_path << "\""
         << ",\"duration_ms\":" << cpu.duration << "},";
    json << "\"heap\":{\"running\":" << (heap.is_running ? "true" : "false")
         << ",\"output_path\":\"" << heap.output_path << "\""
         << ",\"duration_ms\":" << heap.duration << "},";
    json << "\"growth\":{\"running\":" << (growth.is_running ? "true" : "false")
         << ",\"output_path\":\"" << growth.output_path << "\""
         << ",\"duration_ms\":" << growth.duration << "}";
    json << "}";

    return HandlerResponse::json(json.str());
}

// --- CPU endpoints ---

HandlerResponse ProfilerHttpHandlers::handleCpuAnalyze(int duration, const std::string& output_type) {
    duration = clampDuration(duration, 1, 300);

    if (!validateOutputType(output_type)) {
        return errorResp(400, "Invalid output_type. Must be 'flamegraph' or 'pprof'");
    }

    std::string svg = profiler_.analyzeCPUProfile(duration, output_type);

    if (svg.size() > 10 && svg[0] == '{' && svg[1] == '"') {
        return errorResp(500, svg);
    }

    return HandlerResponse::svg(svg);
}

HandlerResponse ProfilerHttpHandlers::handleCpuSvgRaw(int duration) {
    duration = clampDuration(duration, 1, 300);

    std::string profile_data = profiler_.getRawCPUProfile(duration);
    if (profile_data.empty()) {
        return errorResp(500, "Failed to generate CPU profile");
    }

    std::string temp_file = "/tmp/cpu_raw.prof";
    {
        std::ofstream out(temp_file, std::ios::binary);
        out.write(profile_data.data(), profile_data.size());
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
    std::string svg;
    profiler_.executeCommand(cmd, svg);

    size_t pos = svg.find("<?xml");
    if (pos == std::string::npos) pos = svg.find("<svg");
    if (pos != std::string::npos && pos > 0) svg = svg.substr(pos);

    if (svg.empty() || svg.find("<svg") == std::string::npos) {
        return errorResp(500, "Failed to generate SVG: insufficient CPU samples collected.");
    }

    auto resp = HandlerResponse::svg(svg);
    resp.headers["Content-Disposition"] = "attachment; filename=cpu_profile.svg";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handleCpuFlamegraphRaw(int duration) {
    duration = clampDuration(duration, 1, 300);

    std::string profile_data = profiler_.getRawCPUProfile(duration);
    if (profile_data.empty()) {
        return errorResp(500, "Failed to generate CPU profile");
    }

    std::string temp_file = "/tmp/cpu_raw.prof";
    {
        std::ofstream out(temp_file, std::ios::binary);
        out.write(profile_data.data(), profile_data.size());
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string collapsed_file = "/tmp/cpu_collapsed.prof";

    std::ostringstream cmd;
    cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > " << collapsed_file << " 2>&1";
    std::string out;
    if (!profiler_.executeCommand(cmd.str(), out)) {
        return errorResp(500, "Failed to execute pprof --collapsed command");
    }

    // Verify collapsed data
    std::ifstream in(collapsed_file);
    if (!in.is_open()) return errorResp(500, "Failed to create collapsed file");
    std::string line;
    bool has_data = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') { has_data = true; break; }
    }
    in.close();
    if (!has_data) return errorResp(500, "pprof --collapsed produced no data.");

    std::string fg_cmd = "perl ./flamegraph.pl --title=\"CPU Flame Graph\" --width=1200 " + collapsed_file + " 2>/dev/null";
    std::string svg;
    profiler_.executeCommand(fg_cmd, svg);

    if (svg.find("<?xml") == std::string::npos && svg.find("<svg") == std::string::npos) {
        return errorResp(500, "Failed to generate FlameGraph: insufficient CPU samples.");
    }

    auto resp = HandlerResponse::svg(svg);
    resp.headers["Content-Disposition"] = "attachment; filename=cpu_flamegraph_" + std::to_string(duration) + "s.svg";
    return resp;
}

// --- Heap endpoints ---

HandlerResponse ProfilerHttpHandlers::handleHeapAnalyze(const std::string& output_type) {
    if (!validateOutputType(output_type)) {
        return errorResp(400, "Invalid output_type. Must be 'flamegraph' or 'pprof'");
    }

    std::string svg = profiler_.analyzeHeapProfile(1, output_type);

    if (svg.size() > 10 && svg[0] == '{' && svg[1] == '"') {
        return errorResp(500, "Failed to generate heap flame graph");
    }

    return HandlerResponse::svg(svg);
}

HandlerResponse ProfilerHttpHandlers::handleHeapSvgRaw() {
    std::string heap_sample = profiler_.getRawHeapSample();
    if (heap_sample.empty()) {
        return errorResp(500, "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER is set.");
    }

    std::string temp_file = "/tmp/heap_raw.prof";
    {
        std::ofstream out(temp_file);
        out << heap_sample;
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
    std::string svg;
    profiler_.executeCommand(cmd, svg);

    size_t pos = svg.find("<?xml");
    if (pos == std::string::npos) pos = svg.find("<svg");
    if (pos != std::string::npos && pos > 0) svg = svg.substr(pos);

    if (svg.empty() || svg.find("<svg") == std::string::npos) {
        return errorResp(500, "Failed to generate SVG");
    }

    auto resp = HandlerResponse::svg(svg);
    resp.headers["Content-Disposition"] = "attachment; filename=heap_profile.svg";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handleHeapFlamegraphRaw() {
    std::string heap_sample = profiler_.getRawHeapSample();
    if (heap_sample.empty()) {
        return errorResp(500, "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER is set.");
    }

    std::string temp_file = "/tmp/heap_raw.prof";
    {
        std::ofstream out(temp_file);
        out << heap_sample;
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string collapsed_file = "/tmp/heap_collapsed.prof";

    std::ostringstream cmd;
    cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > " << collapsed_file << " 2>&1";
    std::string out;
    if (!profiler_.executeCommand(cmd.str(), out)) {
        return errorResp(500, "Failed to execute pprof --collapsed command");
    }

    std::ifstream in(collapsed_file);
    if (!in.is_open()) return errorResp(500, "Failed to create collapsed file");
    std::string line;
    bool has_data = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') { has_data = true; break; }
    }
    in.close();
    if (!has_data) return errorResp(500, "pprof --collapsed produced no data");

    std::string fg_cmd = "perl ./flamegraph.pl --title=\"Heap Flame Graph\" --width=1200 " + collapsed_file + " 2>/dev/null";
    std::string svg;
    profiler_.executeCommand(fg_cmd, svg);

    if (svg.find("<?xml") == std::string::npos && svg.find("<svg") == std::string::npos) {
        return errorResp(500, "Failed to generate FlameGraph");
    }

    auto resp = HandlerResponse::svg(svg);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count());
    resp.headers["Content-Disposition"] = "attachment; filename=heap_flamegraph_" + ts + ".svg";
    return resp;
}

// --- Growth endpoints ---

HandlerResponse ProfilerHttpHandlers::handleGrowthAnalyze(const std::string& output_type) {
    if (!validateOutputType(output_type)) {
        return errorResp(400, "Invalid output_type. Must be 'flamegraph' or 'pprof'");
    }

    std::string growth = profiler_.getRawHeapGrowthStacks();
    if (growth.empty()) {
        return errorResp(500, "Failed to get heap growth stacks. No heap growth data available.");
    }

    std::string temp_file = "/tmp/growth_sample.prof";
    {
        std::ofstream out(temp_file);
        out << growth;
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string svg;

    if (output_type == "flamegraph") {
        std::string collapsed_file = "/tmp/growth_collapsed.prof";
        std::ostringstream cmd;
        cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > " << collapsed_file << " 2>/dev/null";

        std::string out;
        if (!profiler_.executeCommand(cmd.str(), out)) {
            return errorResp(500, "Failed to generate collapsed format");
        }

        std::ostringstream fg;
        fg << "perl ./flamegraph.pl --title=\"Heap Growth Flame Graph\" --width=1200 " << collapsed_file << " 2>/dev/null";
        if (!profiler_.executeCommand(fg.str(), svg)) {
            return errorResp(500, "Failed to execute flamegraph.pl command");
        }

        if (svg.find("<?xml") == std::string::npos && svg.find("<svg") == std::string::npos) {
            return errorResp(500, "flamegraph.pl did not generate valid SVG");
        }
    } else {
        std::ostringstream cmd;
        cmd << "./pprof --svg " << exe_path << " " << temp_file << " 2>&1";
        if (!profiler_.executeCommand(cmd.str(), svg)) {
            return errorResp(500, "Failed to execute pprof command");
        }

        size_t svg_start = svg.find("<svg");
        if (svg_start != std::string::npos) {
            size_t tag_end = svg.find(">", svg_start);
            if (tag_end != std::string::npos) {
                std::string tag = svg.substr(svg_start, tag_end - svg_start);
                if (tag.find("viewBox") == std::string::npos) {
                    svg.insert(tag_end, " viewBox=\"0 -1000 2000 1000\"");
                }
            }
        }
    }

    return HandlerResponse::svg(svg);
}

HandlerResponse ProfilerHttpHandlers::handleGrowthSvgRaw() {
    std::string growth = profiler_.getRawHeapGrowthStacks();
    if (growth.empty()) {
        return errorResp(500, "Failed to get heap growth stacks. No heap growth data available.");
    }

    std::string temp_file = "/tmp/growth_raw.prof";
    {
        std::ofstream out(temp_file);
        out << growth;
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
    std::string svg;
    profiler_.executeCommand(cmd, svg);

    size_t pos = svg.find("<?xml");
    if (pos == std::string::npos) pos = svg.find("<svg");
    if (pos != std::string::npos && pos > 0) svg = svg.substr(pos);

    if (svg.empty() || svg.find("<svg") == std::string::npos) {
        return errorResp(500, "Failed to generate SVG");
    }

    auto resp = HandlerResponse::svg(svg);
    resp.headers["Content-Disposition"] = "attachment; filename=growth_profile.svg";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handleGrowthFlamegraphRaw() {
    std::string growth = profiler_.getRawHeapGrowthStacks();
    if (growth.empty()) {
        return errorResp(500, "Failed to get heap growth stacks. No heap growth data available.");
    }

    std::string temp_file = "/tmp/growth_raw.prof";
    {
        std::ofstream out(temp_file);
        out << growth;
    }

    std::string exe_path = profiler_.getExecutablePath();
    std::string collapsed_file = "/tmp/growth_collapsed.prof";

    std::ostringstream cmd;
    cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > " << collapsed_file << " 2>&1";
    std::string out;
    if (!profiler_.executeCommand(cmd.str(), out)) {
        return errorResp(500, "Failed to execute pprof --collapsed command");
    }

    std::ifstream in(collapsed_file);
    if (!in.is_open()) return errorResp(500, "Failed to create collapsed file");
    std::string line;
    bool has_data = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') { has_data = true; break; }
    }
    in.close();
    if (!has_data) return errorResp(500, "pprof --collapsed produced no data");

    std::string fg_cmd = "perl ./flamegraph.pl --title=\"Heap Growth Flame Graph\" --width=1200 " + collapsed_file + " 2>/dev/null";
    std::string svg;
    profiler_.executeCommand(fg_cmd, svg);

    if (svg.find("<?xml") == std::string::npos && svg.find("<svg") == std::string::npos) {
        return errorResp(500, "Failed to generate FlameGraph");
    }

    auto resp = HandlerResponse::svg(svg);
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count());
    resp.headers["Content-Disposition"] = "attachment; filename=growth_flamegraph_" + ts + ".svg";
    return resp;
}

// --- Standard pprof ---

HandlerResponse ProfilerHttpHandlers::handlePprofProfile(int seconds) {
    seconds = clampDuration(seconds, 1, 300);

    std::string data = profiler_.getRawCPUProfile(seconds);
    if (data.empty()) {
        return errorResp(500, "Failed to generate CPU profile");
    }

    auto resp = HandlerResponse::binary(data, "profile");
    resp.content_type = "application/octet-stream";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handlePprofHeap() {
    std::string data = profiler_.getRawHeapSample();
    if (data.empty()) {
        return errorResp(500, "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER is set.");
    }

    HandlerResponse resp;
    resp.status = 200;
    resp.content_type = "text/plain";
    resp.body = data;
    resp.headers["Content-Disposition"] = "attachment; filename=heap";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handlePprofGrowth() {
    std::string data = profiler_.getRawHeapGrowthStacks();
    if (data.empty()) {
        return errorResp(500, "Failed to get heap growth stacks. No heap growth data available.");
    }

    HandlerResponse resp;
    resp.status = 200;
    resp.content_type = "text/plain";
    resp.body = data;
    resp.headers["Content-Disposition"] = "attachment; filename=growth";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handlePprofSymbol(const std::string& body) {
    std::istringstream iss(body);
    std::string address;
    std::ostringstream result;

    while (std::getline(iss, address)) {
        if (address.empty() || address[0] == '#') continue;

        std::string original = address;
        std::string addr_str = address;
        if (addr_str.size() > 2 && addr_str[0] == '0' && addr_str[1] == 'x') {
            addr_str = addr_str.substr(2);
        }

        try {
            uintptr_t addr = std::stoull(addr_str, nullptr, 16);
            std::string symbol = profiler_.resolveSymbolWithBackward(reinterpret_cast<void*>(addr));
            result << original << " " << symbol << "\n";
        } catch (...) {
            result << original << " " << original << "\n";
        }
    }

    return HandlerResponse::text(result.str());
}

// --- Thread stacks ---

HandlerResponse ProfilerHttpHandlers::handleThreadStacks() {
    std::string stacks = profiler_.getThreadCallStacks();
    if (stacks.empty()) {
        return errorResp(500, "Failed to get thread call stacks");
    }
    return HandlerResponse::text(stacks);
}

PROFILER_NAMESPACE_END
