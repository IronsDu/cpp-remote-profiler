/// @file web_server.cpp
/// @brief HTTP route handlers using the HttpServer abstraction

#include "web_server.h"
#include "internal/web_resources.h"
#include "profiler_manager.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

PROFILER_NAMESPACE_BEGIN

/// Helper: build a JSON error response
static void setErrorResponse(Response& resp, int status, const std::string& message) {
    resp.status_code = status;
    resp.setJson("{\"error\":\"" + message + "\"}");
}

void registerHttpHandlers(profiler::ProfilerManager& profiler, HttpServer& server) {

    // ---- Static pages ----

    // Status endpoint
    server.get("/api/status",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   auto cpuState = profiler.getProfilerState(profiler::ProfilerType::CPU);
                   auto heapState = profiler.getProfilerState(profiler::ProfilerType::HEAP);
                   auto growthState = profiler.getProfilerState(profiler::ProfilerType::HEAP_GROWTH);

                   std::ostringstream json;
                   json << "{";
                   json << "\"cpu\":{\"running\":"
                        << (cpuState.is_running ? "true" : "false")
                        << ",\"output_path\":\"" << cpuState.output_path << "\""
                        << ",\"duration_ms\":" << cpuState.duration << "},";
                   json << "\"heap\":{\"running\":"
                        << (heapState.is_running ? "true" : "false")
                        << ",\"output_path\":\"" << heapState.output_path << "\""
                        << ",\"duration_ms\":" << heapState.duration << "},";
                   json << "\"growth\":{\"running\":"
                        << (growthState.is_running ? "true" : "false")
                        << ",\"output_path\":\"" << growthState.output_path << "\""
                        << ",\"duration_ms\":" << growthState.duration << "}";
                   json << "}";

                   resp.setJson(json.str());
               });

    // Index page
    server.get("/",
               []([[maybe_unused]] const Request& req, Response& resp) {
                   resp.setHtml(WebResources::getIndexPage());
               });

    // CPU SVG viewer page
    server.get("/show_svg.html",
               []([[maybe_unused]] const Request& req, Response& resp) {
                   resp.setHtml(WebResources::getCpuSvgViewerPage());
               });

    // Heap SVG viewer page
    server.get("/show_heap_svg.html",
               []([[maybe_unused]] const Request& req, Response& resp) {
                   resp.setHtml(WebResources::getHeapSvgViewerPage());
               });

    // Growth SVG viewer page
    server.get("/show_growth_svg.html",
               []([[maybe_unused]] const Request& req, Response& resp) {
                   resp.setHtml(WebResources::getGrowthSvgViewerPage());
               });

    // ---- CPU analyze endpoint ----
    server.get("/api/cpu/analyze",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   // Parse duration parameter
                   int duration = 10;
                   auto it = req.params.find("duration");
                   if (it != req.params.end() && !it->second.empty()) {
                       try {
                           duration = std::stoi(it->second);
                           if (duration < 1)
                               duration = 1;
                           if (duration > 300)
                               duration = 300;
                       } catch (...) {
                           setErrorResponse(resp, 400, "Invalid duration parameter");
                           return;
                       }
                   }

                   std::string output_type = "pprof";
                   auto ot_it = req.params.find("output_type");
                   if (ot_it != req.params.end() && !ot_it->second.empty()) {
                       output_type = ot_it->second;
                   }

                   if (output_type != "flamegraph" && output_type != "pprof") {
                       setErrorResponse(resp, 400, "Invalid output_type. Must be 'flamegraph' or 'pprof'");
                       return;
                   }

                   std::cout << "Starting CPU analysis: duration=" << duration << "s, output_type=" << output_type
                             << std::endl;

                   std::string svg_content = profiler.analyzeCPUProfile(duration, output_type);

                   if (svg_content.size() > 10 && svg_content[0] == '{' && svg_content[1] == '"') {
                       setErrorResponse(resp, 500, svg_content);
                       return;
                   }

                   resp.setSvg(svg_content);
               });

    // ---- Heap analyze endpoint ----
    server.get("/api/heap/analyze",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Starting Heap analysis..." << std::endl;

                   std::string output_type = "pprof";
                   auto ot_it = req.params.find("output_type");
                   if (ot_it != req.params.end() && !ot_it->second.empty()) {
                       output_type = ot_it->second;
                   }

                   if (output_type != "flamegraph" && output_type != "pprof") {
                       setErrorResponse(resp, 400, "Invalid output_type. Must be 'flamegraph' or 'pprof'");
                       return;
                   }

                   std::string svg_content = profiler.analyzeHeapProfile(1, output_type);

                   if (svg_content.size() > 10 && svg_content[0] == '{' && svg_content[1] == '"') {
                       setErrorResponse(resp, 500, "Failed to generate heap flame graph");
                       return;
                   }

                   resp.setSvg(svg_content);
               });

    // ---- Growth analyze endpoint ----
    server.get("/api/growth/analyze",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Starting Heap Growth analysis..." << std::endl;

                   std::string output_type = "pprof";
                   auto ot_it = req.params.find("output_type");
                   if (ot_it != req.params.end() && !ot_it->second.empty()) {
                       output_type = ot_it->second;
                   }

                   if (output_type != "flamegraph" && output_type != "pprof") {
                       setErrorResponse(resp, 400, "Invalid output_type. Must be 'flamegraph' or 'pprof'");
                       return;
                   }

                   std::string growth_stacks = profiler.getRawHeapGrowthStacks();
                   if (growth_stacks.empty()) {
                       setErrorResponse(resp, 500,
                                        "Failed to get heap growth stacks. No heap growth data available.");
                       return;
                   }

                   std::string temp_file = "/tmp/growth_sample.prof";
                   {
                       std::ofstream out(temp_file);
                       out << growth_stacks;
                   }

                   std::string exe_path = profiler.getExecutablePath();
                   std::string svg_output;

                   if (output_type == "flamegraph") {
                       std::cout << "Generating Growth FlameGraph..." << std::endl;

                       std::string collapsed_file = "/tmp/growth_collapsed.prof";
                       std::ostringstream collapsed_cmd;
                       collapsed_cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > "
                                     << collapsed_file << " 2>/dev/null";

                       std::string collapsed_output;
                       if (!profiler.executeCommand(collapsed_cmd.str(), collapsed_output)) {
                           setErrorResponse(resp, 500, "Failed to generate collapsed format");
                           return;
                       }

                       std::ostringstream flamegraph_cmd;
                       flamegraph_cmd << "perl ./flamegraph.pl"
                                      << " --title=\"Heap Growth Flame Graph\""
                                      << " --width=1200"
                                      << " " << collapsed_file << " 2>/dev/null";

                       if (!profiler.executeCommand(flamegraph_cmd.str(), svg_output)) {
                           setErrorResponse(resp, 500, "Failed to execute flamegraph.pl command");
                           return;
                       }

                       if (svg_output.find("<?xml") == std::string::npos &&
                           svg_output.find("<svg") == std::string::npos) {
                           setErrorResponse(resp, 500, "flamegraph.pl did not generate valid SVG");
                           return;
                       }
                   } else {
                       std::cout << "Generating Growth pprof SVG..." << std::endl;

                       std::ostringstream cmd;
                       cmd << "./pprof --svg " << exe_path << " " << temp_file << " 2>&1";

                       if (!profiler.executeCommand(cmd.str(), svg_output)) {
                           setErrorResponse(resp, 500, "Failed to execute pprof command");
                           return;
                       }

                       size_t svg_start = svg_output.find("<svg");
                       if (svg_start != std::string::npos) {
                           size_t svg_tag_end = svg_output.find(">", svg_start);
                           if (svg_tag_end != std::string::npos) {
                               std::string svg_tag = svg_output.substr(svg_start, svg_tag_end - svg_start);
                               if (svg_tag.find("viewBox") == std::string::npos) {
                                   svg_output.insert(svg_tag_end, " viewBox=\"0 -1000 2000 1000\"");
                               }
                           }
                       }
                   }

                   resp.setSvg(svg_output);
               });

    // ---- CPU raw SVG endpoint ----
    server.get("/api/cpu/svg_raw",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   int duration = 10;
                   auto it = req.params.find("duration");
                   if (it != req.params.end() && !it->second.empty()) {
                       try {
                           duration = std::stoi(it->second);
                           if (duration < 1)
                               duration = 1;
                           if (duration > 300)
                               duration = 300;
                       } catch (...) {
                           setErrorResponse(resp, 400, "Invalid duration parameter");
                           return;
                       }
                   }

                   std::cout << "Starting CPU raw SVG generation: duration=" << duration << "s" << std::endl;

                   std::string profile_data = profiler.getRawCPUProfile(duration);
                   if (profile_data.empty()) {
                       setErrorResponse(resp, 500, "Failed to generate CPU profile");
                       return;
                   }

                   std::string temp_file = "/tmp/cpu_raw.prof";
                   {
                       std::ofstream out(temp_file, std::ios::binary);
                       out.write(profile_data.data(), profile_data.size());
                   }

                   std::string exe_path = profiler.getExecutablePath();
                   std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
                   std::string svg_content;
                   profiler.executeCommand(cmd, svg_content);

                   // Strip pprof info output
                   size_t svg_start = svg_content.find("<?xml");
                   if (svg_start == std::string::npos) {
                       svg_start = svg_content.find("<svg");
                   }
                   if (svg_start != std::string::npos && svg_start > 0) {
                       svg_content = svg_content.substr(svg_start);
                   }

                   if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                       setErrorResponse(resp, 500,
                                        "Failed to generate SVG: insufficient CPU samples collected. "
                                        "Please try a longer sampling duration (at least 5 seconds recommended).");
                       return;
                   }

                   resp.setSvg(svg_content);
                   resp.headers["Content-Disposition"] = "attachment; filename=cpu_profile.svg";
               });

    // ---- CPU FlameGraph raw SVG endpoint ----
    server.get("/api/cpu/flamegraph_raw",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   int duration = 10;
                   auto it = req.params.find("duration");
                   if (it != req.params.end() && !it->second.empty()) {
                       try {
                           duration = std::stoi(it->second);
                           if (duration < 1)
                               duration = 1;
                           if (duration > 300)
                               duration = 300;
                       } catch (...) {
                           setErrorResponse(resp, 400, "Invalid duration parameter");
                           return;
                       }
                   }

                   std::cout << "Starting CPU FlameGraph generation: duration=" << duration << "s" << std::endl;

                   std::string profile_data = profiler.getRawCPUProfile(duration);
                   if (profile_data.empty()) {
                       setErrorResponse(resp, 500, "Failed to generate CPU profile");
                       return;
                   }

                   std::string temp_file = "/tmp/cpu_raw.prof";
                   {
                       std::ofstream out(temp_file, std::ios::binary);
                       out.write(profile_data.data(), profile_data.size());
                   }

                   std::string exe_path = profiler.getExecutablePath();

                   // Generate collapsed format
                   std::string collapsed_file = "/tmp/cpu_collapsed.prof";
                   std::ostringstream collapsed_cmd;
                   collapsed_cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > "
                                 << collapsed_file << " 2>&1";

                   std::string collapsed_output;
                   if (!profiler.executeCommand(collapsed_cmd.str(), collapsed_output)) {
                       setErrorResponse(resp, 500, "Failed to execute pprof --collapsed command");
                       return;
                   }

                   // Verify collapsed data
                   std::ifstream collapsed_in(collapsed_file);
                   if (!collapsed_in.is_open()) {
                       setErrorResponse(resp, 500, "Failed to create collapsed file");
                       return;
                   }
                   std::string line;
                   bool has_data = false;
                   while (std::getline(collapsed_in, line)) {
                       if (!line.empty() && line[0] != '#') {
                           has_data = true;
                           break;
                       }
                   }
                   collapsed_in.close();

                   if (!has_data) {
                       setErrorResponse(resp, 500,
                                        "pprof --collapsed produced no data. Please try a longer sampling duration.");
                       return;
                   }

                   // Generate FlameGraph
                   std::string cmd =
                       "perl ./flamegraph.pl --title=\"CPU Flame Graph\" --width=1200 " + collapsed_file + " 2>/dev/null";
                   std::string svg_content;
                   profiler.executeCommand(cmd, svg_content);

                   if (svg_content.find("<?xml") == std::string::npos && svg_content.find("<svg") == std::string::npos) {
                       setErrorResponse(resp, 500,
                                        "Failed to generate FlameGraph: insufficient CPU samples collected. "
                                        "Please try a longer sampling duration (at least 5 seconds recommended).");
                       return;
                   }

                   resp.setSvg(svg_content);
                   resp.headers["Content-Disposition"] =
                       "attachment; filename=cpu_flamegraph_" + std::to_string(duration) + "s.svg";
               });

    // ---- Heap raw SVG endpoint ----
    server.get("/api/heap/svg_raw",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Starting Heap raw SVG generation..." << std::endl;

                   std::string heap_sample = profiler.getRawHeapSample();
                   if (heap_sample.empty()) {
                       setErrorResponse(resp, 500,
                                        "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER "
                                        "environment variable is set.");
                       return;
                   }

                   std::string temp_file = "/tmp/heap_raw.prof";
                   {
                       std::ofstream out(temp_file);
                       out << heap_sample;
                   }

                   std::string exe_path = profiler.getExecutablePath();
                   std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
                   std::string svg_content;
                   profiler.executeCommand(cmd, svg_content);

                   size_t svg_start = svg_content.find("<?xml");
                   if (svg_start == std::string::npos) {
                       svg_start = svg_content.find("<svg");
                   }
                   if (svg_start != std::string::npos && svg_start > 0) {
                       svg_content = svg_content.substr(svg_start);
                   }

                   if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                       setErrorResponse(resp, 500, "Failed to generate SVG");
                       return;
                   }

                   resp.setSvg(svg_content);
                   resp.headers["Content-Disposition"] = "attachment; filename=heap_profile.svg";
               });

    // ---- Heap FlameGraph raw SVG endpoint ----
    server.get("/api/heap/flamegraph_raw",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Starting Heap FlameGraph generation..." << std::endl;

                   std::string heap_sample = profiler.getRawHeapSample();
                   if (heap_sample.empty()) {
                       setErrorResponse(resp, 500,
                                        "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER "
                                        "environment variable is set.");
                       return;
                   }

                   std::string temp_file = "/tmp/heap_raw.prof";
                   {
                       std::ofstream out(temp_file);
                       out << heap_sample;
                   }

                   std::string exe_path = profiler.getExecutablePath();

                   std::string collapsed_file = "/tmp/heap_collapsed.prof";
                   std::ostringstream collapsed_cmd;
                   collapsed_cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > "
                                 << collapsed_file << " 2>&1";

                   std::string collapsed_output;
                   if (!profiler.executeCommand(collapsed_cmd.str(), collapsed_output)) {
                       setErrorResponse(resp, 500, "Failed to execute pprof --collapsed command");
                       return;
                   }

                   std::ifstream collapsed_in(collapsed_file);
                   if (!collapsed_in.is_open()) {
                       setErrorResponse(resp, 500, "Failed to create collapsed file");
                       return;
                   }
                   std::string line;
                   bool has_data = false;
                   while (std::getline(collapsed_in, line)) {
                       if (!line.empty() && line[0] != '#') {
                           has_data = true;
                           break;
                       }
                   }
                   collapsed_in.close();

                   if (!has_data) {
                       setErrorResponse(resp, 500, "pprof --collapsed produced no data");
                       return;
                   }

                   std::string cmd =
                       "perl ./flamegraph.pl --title=\"Heap Flame Graph\" --width=1200 " + collapsed_file + " 2>/dev/null";
                   std::string svg_content;
                   profiler.executeCommand(cmd, svg_content);

                   if (svg_content.find("<?xml") == std::string::npos && svg_content.find("<svg") == std::string::npos) {
                       setErrorResponse(resp, 500, "Failed to generate FlameGraph");
                       return;
                   }

                   resp.setSvg(svg_content);
                   std::string timestamp = std::to_string(
                       std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
                   resp.headers["Content-Disposition"] = "attachment; filename=heap_flamegraph_" + timestamp + ".svg";
               });

    // ---- Growth raw SVG endpoint ----
    server.get("/api/growth/svg_raw",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Starting Growth raw SVG generation..." << std::endl;

                   std::string heap_growth = profiler.getRawHeapGrowthStacks();
                   if (heap_growth.empty()) {
                       setErrorResponse(resp, 500, "Failed to get heap growth stacks. No heap growth data available.");
                       return;
                   }

                   std::string temp_file = "/tmp/growth_raw.prof";
                   {
                       std::ofstream out(temp_file);
                       out << heap_growth;
                   }

                   std::string exe_path = profiler.getExecutablePath();
                   std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
                   std::string svg_content;
                   profiler.executeCommand(cmd, svg_content);

                   size_t svg_start = svg_content.find("<?xml");
                   if (svg_start == std::string::npos) {
                       svg_start = svg_content.find("<svg");
                   }
                   if (svg_start != std::string::npos && svg_start > 0) {
                       svg_content = svg_content.substr(svg_start);
                   }

                   if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                       setErrorResponse(resp, 500, "Failed to generate SVG");
                       return;
                   }

                   resp.setSvg(svg_content);
                   resp.headers["Content-Disposition"] = "attachment; filename=growth_profile.svg";
               });

    // ---- Growth FlameGraph raw SVG endpoint ----
    server.get("/api/growth/flamegraph_raw",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Starting Growth FlameGraph generation..." << std::endl;

                   std::string heap_growth = profiler.getRawHeapGrowthStacks();
                   if (heap_growth.empty()) {
                       setErrorResponse(resp, 500, "Failed to get heap growth stacks. No heap growth data available.");
                       return;
                   }

                   std::string temp_file = "/tmp/growth_raw.prof";
                   {
                       std::ofstream out(temp_file);
                       out << heap_growth;
                   }

                   std::string exe_path = profiler.getExecutablePath();

                   std::string collapsed_file = "/tmp/growth_collapsed.prof";
                   std::ostringstream collapsed_cmd;
                   collapsed_cmd << "./pprof --collapsed " << exe_path << " " << temp_file << " > "
                                 << collapsed_file << " 2>&1";

                   std::string collapsed_output;
                   if (!profiler.executeCommand(collapsed_cmd.str(), collapsed_output)) {
                       setErrorResponse(resp, 500, "Failed to execute pprof --collapsed command");
                       return;
                   }

                   std::ifstream collapsed_in(collapsed_file);
                   if (!collapsed_in.is_open()) {
                       setErrorResponse(resp, 500, "Failed to create collapsed file");
                       return;
                   }
                   std::string line;
                   bool has_data = false;
                   while (std::getline(collapsed_in, line)) {
                       if (!line.empty() && line[0] != '#') {
                           has_data = true;
                           break;
                       }
                   }
                   collapsed_in.close();

                   if (!has_data) {
                       setErrorResponse(resp, 500, "pprof --collapsed produced no data");
                       return;
                   }

                   std::string cmd =
                       "perl ./flamegraph.pl --title=\"Heap Growth Flame Graph\" --width=1200 " + collapsed_file +
                       " 2>/dev/null";
                   std::string svg_content;
                   profiler.executeCommand(cmd, svg_content);

                   if (svg_content.find("<?xml") == std::string::npos && svg_content.find("<svg") == std::string::npos) {
                       setErrorResponse(resp, 500, "Failed to generate FlameGraph");
                       return;
                   }

                   resp.setSvg(svg_content);
                   std::string timestamp = std::to_string(
                       std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
                   resp.headers["Content-Disposition"] = "attachment; filename=growth_flamegraph_" + timestamp + ".svg";
               });

    // ---- Symbol resolution endpoint ----
    server.post("/pprof/symbol",
                [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                    std::istringstream iss(req.body);
                    std::string address;
                    std::ostringstream result;

                    while (std::getline(iss, address)) {
                        if (address.empty() || address[0] == '#') {
                            continue;
                        }

                        std::string original_addr = address;
                        std::string addr_str = address;
                        if (addr_str.size() > 2 && addr_str[0] == '0' && addr_str[1] == 'x') {
                            addr_str = addr_str.substr(2);
                        }

                        try {
                            uintptr_t addr = std::stoull(addr_str, nullptr, 16);
                            void* ptr = reinterpret_cast<void*>(addr);
                            std::string symbol = profiler.resolveSymbolWithBackward(ptr);
                            result << original_addr << " " << symbol << "\n";
                        } catch (...) {
                            result << original_addr << " " << original_addr << "\n";
                        }
                    }

                    resp.body = result.str();
                    resp.content_type = "text/plain";
                });

    // ---- Standard pprof: /pprof/profile ----
    server.get("/pprof/profile",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   int seconds = 30;
                   auto it = req.params.find("seconds");
                   if (it != req.params.end() && !it->second.empty()) {
                       try {
                           seconds = std::stoi(it->second);
                           if (seconds < 1)
                               seconds = 1;
                           if (seconds > 300)
                               seconds = 300;
                       } catch (...) {
                           resp.status_code = 400;
                           resp.body = "Invalid seconds parameter";
                           return;
                       }
                   }

                   std::cout << "Received /pprof/profile request, seconds=" << seconds << std::endl;

                   std::string profile_data = profiler.getRawCPUProfile(seconds);
                   if (profile_data.empty()) {
                       resp.status_code = 500;
                       resp.body = "Failed to generate CPU profile";
                       return;
                   }

                   resp.setBinary(profile_data, "profile");
               });

    // ---- Standard pprof: /pprof/heap ----
    server.get("/pprof/heap",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Received /pprof/heap request" << std::endl;

                   std::string heap_sample = profiler.getRawHeapSample();
                   if (heap_sample.empty()) {
                       resp.status_code = 500;
                       resp.body = "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER is set.";
                       return;
                   }

                   resp.body = heap_sample;
                   resp.content_type = "text/plain";
                   resp.headers["Content-Disposition"] = "attachment; filename=heap";
               });

    // ---- Standard pprof: /pprof/growth ----
    server.get("/pprof/growth",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Received /pprof/growth request" << std::endl;

                   std::string heap_growth = profiler.getRawHeapGrowthStacks();
                   if (heap_growth.empty()) {
                       resp.status_code = 500;
                       resp.body = "Failed to get heap growth stacks. No heap growth data available.";
                       return;
                   }

                   resp.body = heap_growth;
                   resp.content_type = "text/plain";
                   resp.headers["Content-Disposition"] = "attachment; filename=growth";
               });

    // ---- Thread stacks endpoint ----
    server.get("/api/thread/stacks",
               [&profiler]([[maybe_unused]] const Request& req, Response& resp) {
                   std::cout << "Received /api/thread/stacks request" << std::endl;

                   std::string thread_stacks = profiler.getThreadCallStacks();
                   if (thread_stacks.empty()) {
                       setErrorResponse(resp, 500, "Failed to get thread call stacks");
                       return;
                   }

                   resp.body = thread_stacks;
                   resp.content_type = "text/plain";
               });
}

PROFILER_NAMESPACE_END
