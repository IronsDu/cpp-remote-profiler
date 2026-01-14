#include "web_server.h"
#include "web_resources.h"
#include "profiler_manager.h"
#include <drogon/drogon.h>
#include <sstream>
#include <fstream>
#include <iostream>

using namespace drogon;

namespace profiler {

void registerHttpHandlers(profiler::ProfilerManager& profiler) {
    // Status endpoint - 获取 profiler 状态
    app().registerHandler(
        "/api/status",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value root;
            root["cpu"]["running"] = profiler.isProfilerRunning(profiler::ProfilerType::CPU);
            root["heap"]["running"] = profiler.isProfilerRunning(profiler::ProfilerType::HEAP);
            root["growth"]["running"] = profiler.isProfilerRunning(profiler::ProfilerType::HEAP_GROWTH);

            auto cpuState = profiler.getProfilerState(profiler::ProfilerType::CPU);
            auto heapState = profiler.getProfilerState(profiler::ProfilerType::HEAP);
            auto growthState = profiler.getProfilerState(profiler::ProfilerType::HEAP_GROWTH);

            root["cpu"]["output_path"] = cpuState.output_path;
            root["cpu"]["duration_ms"] = static_cast<Json::Int64>(cpuState.duration);

            root["heap"]["output_path"] = heapState.output_path;
            root["heap"]["duration_ms"] = static_cast<Json::Int64>(heapState.duration);

            root["growth"]["output_path"] = growthState.output_path;
            root["growth"]["duration_ms"] = static_cast<Json::Int64>(growthState.duration);

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        {Get}
    );

    // Index page - 主页面
    app().registerHandler(
        "/",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string html = WebResources::getIndexPage();
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // CPU SVG flame graph viewer page
    app().registerHandler(
        "/show_svg.html",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string html = WebResources::getCpuSvgViewerPage();
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // Heap SVG flame graph viewer page
    app().registerHandler(
        "/show_heap_svg.html",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string html = WebResources::getHeapSvgViewerPage();
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // Growth SVG flame graph viewer page
    app().registerHandler(
        "/show_growth_svg.html",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string html = WebResources::getGrowthSvgViewerPage();
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // CPU analyze endpoint - 一键式CPU分析（使用pprof生成SVG火焰图）
    app().registerHandler(
        "/api/cpu/analyze",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            // 获取参数
            auto duration_param = req->getParameter("duration");
            auto output_type_param = req->getParameter("output_type");

            // 默认值
            int duration = 10;  // 默认10秒
            if (!duration_param.empty()) {
                try {
                    duration = std::stoi(duration_param);
                    if (duration < 1) duration = 1;
                    if (duration > 300) duration = 300;  // 最多5分钟
                } catch (const std::exception& e) {
                    Json::Value root;
                    root["error"] = "Invalid duration parameter";
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k400BadRequest);
                    callback(resp);
                    return;
                }
            }

            std::string output_type = "flamegraph";
            if (!output_type_param.empty()) {
                output_type = output_type_param;
            }

            std::cout << "Starting CPU analysis: duration=" << duration
                      << "s, output_type=" << output_type << std::endl;

            // 调用 analyzeCPUProfile（这是阻塞调用，会等待整个profiling过程完成）
            std::string svg_content = profiler.analyzeCPUProfile(duration, output_type);

            // 检查是否是错误响应（JSON格式的错误，更精确的检查）
            if (svg_content.size() > 10 && svg_content[0] == '{' && svg_content[1] == '"') {
                auto resp = HttpResponse::newHttpJsonResponse(Json::Value(svg_content));
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 返回SVG内容
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg_content);
            resp->setContentTypeCode(CT_TEXT_XML);
            resp->addHeader("Content-Type", "image/svg+xml");
            callback(resp);
        },
        {Get, Post}
    );

    // Heap analyze endpoint - 一键式Heap分析（使用pprof生成SVG火焰图）
    app().registerHandler(
        "/api/heap/analyze",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::cout << "Starting Heap analysis (using GetHeapSample)..." << std::endl;

            // 直接获取 heap sample（不需要 duration）
            std::string heap_sample = profiler.getRawHeapSample();

            if (heap_sample.empty()) {
                Json::Value root;
                root["error"] = "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER environment variable is set.";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 保存到临时文件
            std::string temp_file = "/tmp/heap_sample.prof";
            std::ofstream out(temp_file);
            out << heap_sample;
            out.close();

            // 使用 pprof 生成 SVG（使用绝对路径）
            std::string exe_path = profiler.getExecutablePath();
            std::string pprof_path = "./pprof";  // 使用相对路径，程序在 build 目录中运行
            std::string cmd = pprof_path + " --svg " + exe_path + " " + temp_file + " 2>&1";
            std::cout << "Executing: " << cmd << std::endl;
            std::string svg_content;
            profiler.executeCommand(cmd, svg_content);

            // 检查结果（检查是否有 SVG 标签）
            if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                Json::Value root;
                root["error"] = "Failed to generate heap flame graph";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 返回SVG内容
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg_content);
            resp->setContentTypeCode(CT_TEXT_XML);
            resp->addHeader("Content-Type", "image/svg+xml");
            callback(resp);
        },
        {Get}
    );

    // Growth analyze endpoint - 一键式Growth分析（使用pprof生成SVG火焰图）
    app().registerHandler(
        "/api/growth/analyze",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::cout << "Starting Heap Growth analysis (using GetHeapGrowthStacks)..." << std::endl;

            // 直接获取 heap growth stacks（不需要 duration）
            std::string heap_growth = profiler.getRawHeapGrowthStacks();

            if (heap_growth.empty()) {
                Json::Value root;
                root["error"] = "Failed to get heap growth stacks. No heap growth data available.";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 保存到临时文件
            std::string temp_file = "/tmp/growth.prof";
            std::ofstream out(temp_file);
            out << heap_growth;
            out.close();

            // 使用 pprof 生成 SVG（使用绝对路径）
            std::string exe_path = profiler.getExecutablePath();
            std::string pprof_path = "./pprof";  // 使用相对路径，程序在 build 目录中运行
            std::string cmd = pprof_path + " --svg " + exe_path + " " + temp_file + " 2>&1";
            std::cout << "Executing: " << cmd << std::endl;
            std::string svg_content;
            profiler.executeCommand(cmd, svg_content);

            // 检查结果（检查是否有 SVG 标签）
            if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                Json::Value root;
                root["error"] = "Failed to generate growth flame graph";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 返回SVG内容
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg_content);
            resp->setContentTypeCode(CT_TEXT_XML);
            resp->addHeader("Content-Type", "image/svg+xml");
            callback(resp);
        },
        {Get}
    );

    // CPU raw SVG endpoint - 返回pprof生成的原始SVG（不做任何修改）
    app().registerHandler(
        "/api/cpu/svg_raw",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            // 获取参数
            auto duration_param = req->getParameter("duration");

            // 默认值
            int duration = 10;  // 默认10秒
            if (!duration_param.empty()) {
                try {
                    duration = std::stoi(duration_param);
                    if (duration < 1) duration = 1;
                    if (duration > 300) duration = 300;  // 最多5分钟
                } catch (const std::exception& e) {
                    Json::Value root;
                    root["error"] = "Invalid duration parameter";
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k400BadRequest);
                    callback(resp);
                    return;
                }
            }

            std::cout << "Starting CPU raw SVG generation: duration=" << duration << "s" << std::endl;

            // 获取原始 CPU profile
            std::string profile_data = profiler.getRawCPUProfile(duration);

            if (profile_data.empty()) {
                Json::Value root;
                root["error"] = "Failed to generate CPU profile";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 保存到临时文件
            std::string temp_file = "/tmp/cpu_raw.prof";
            std::ofstream out(temp_file, std::ios::binary);
            out.write(profile_data.data(), profile_data.size());
            out.close();

            // 使用 pprof 生成原始 SVG（不做任何修改）
            std::string exe_path = profiler.getExecutablePath();
            std::string pprof_path = "./pprof";
            std::string cmd = pprof_path + " --svg " + exe_path + " " + temp_file + " 2>/dev/null";
            std::cout << "Executing: " << cmd << std::endl;
            std::string svg_content;
            profiler.executeCommand(cmd, svg_content);

            // 去掉 pprof 的信息输出，只保留 SVG 内容
            // pprof 会在 SVG 前输出一些信息，需要找到 SVG 的开始位置
            size_t svg_start = svg_content.find("<?xml");
            if (svg_start == std::string::npos) {
                svg_start = svg_content.find("<svg");
            }
            if (svg_start != std::string::npos && svg_start > 0) {
                svg_content = svg_content.substr(svg_start);
            }

            // 检查结果（检查是否有 SVG 标签）
            if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                Json::Value root;
                root["error"] = "Failed to generate SVG: insufficient CPU samples collected. "
                                "Please try a longer sampling duration (at least 5 seconds recommended).";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 返回原始SVG内容（不做任何修改）
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg_content);
            resp->setContentTypeCode(CT_TEXT_XML);
            resp->addHeader("Content-Type", "image/svg+xml");
            // 添加 Content-Disposition 让浏览器下载文件
            resp->addHeader("Content-Disposition", "attachment; filename=cpu_profile.svg");
            callback(resp);
        },
        {Get}
    );

    // Heap raw SVG endpoint - 返回pprof生成的原始SVG（不做任何修改）
    app().registerHandler(
        "/api/heap/svg_raw",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::cout << "Starting Heap raw SVG generation..." << std::endl;

            // 直接获取 heap sample
            std::string heap_sample = profiler.getRawHeapSample();

            if (heap_sample.empty()) {
                Json::Value root;
                root["error"] = "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER environment variable is set.";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 保存到临时文件
            std::string temp_file = "/tmp/heap_raw.prof";
            std::ofstream out(temp_file);
            out << heap_sample;
            out.close();

            // 使用 pprof 生成原始 SVG（不做任何修改）
            std::string exe_path = profiler.getExecutablePath();
            std::string pprof_path = "./pprof";
            std::string cmd = pprof_path + " --svg " + exe_path + " " + temp_file + " 2>/dev/null";
            std::cout << "Executing: " << cmd << std::endl;
            std::string svg_content;
            profiler.executeCommand(cmd, svg_content);

            // 去掉 pprof 的信息输出，只保留 SVG 内容
            // pprof 会在 SVG 前输出一些信息，需要找到 SVG 的开始位置
            size_t svg_start = svg_content.find("<?xml");
            if (svg_start == std::string::npos) {
                svg_start = svg_content.find("<svg");
            }
            if (svg_start != std::string::npos && svg_start > 0) {
                svg_content = svg_content.substr(svg_start);
            }

            // 检查结果（检查是否有 SVG 标签）
            if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                Json::Value root;
                root["error"] = "Failed to generate SVG";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 返回原始SVG内容（不做任何修改）
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg_content);
            resp->setContentTypeCode(CT_TEXT_XML);
            resp->addHeader("Content-Type", "image/svg+xml");
            // 添加 Content-Disposition 让浏览器下载文件
            resp->addHeader("Content-Disposition", "attachment; filename=heap_profile.svg");
            callback(resp);
        },
        {Get}
    );

    // Growth raw SVG endpoint - 返回pprof生成的原始SVG（不做任何修改）
    app().registerHandler(
        "/api/growth/svg_raw",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::cout << "Starting Growth raw SVG generation..." << std::endl;

            // 直接获取 heap growth stacks
            std::string heap_growth = profiler.getRawHeapGrowthStacks();

            if (heap_growth.empty()) {
                Json::Value root;
                root["error"] = "Failed to get heap growth stacks. No heap growth data available.";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 保存到临时文件
            std::string temp_file = "/tmp/growth_raw.prof";
            std::ofstream out(temp_file);
            out << heap_growth;
            out.close();

            // 使用 pprof 生成原始 SVG（不做任何修改）
            std::string exe_path = profiler.getExecutablePath();
            std::string pprof_path = "./pprof";
            std::string cmd = pprof_path + " --svg " + exe_path + " " + temp_file + " 2>/dev/null";
            std::cout << "Executing: " << cmd << std::endl;
            std::string svg_content;
            profiler.executeCommand(cmd, svg_content);

            // 去掉 pprof 的信息输出，只保留 SVG 内容
            // pprof 会在 SVG 前输出一些信息，需要找到 SVG 的开始位置
            size_t svg_start = svg_content.find("<?xml");
            if (svg_start == std::string::npos) {
                svg_start = svg_content.find("<svg");
            }
            if (svg_start != std::string::npos && svg_start > 0) {
                svg_content = svg_content.substr(svg_start);
            }

            // 检查结果（检查是否有 SVG 标签）
            if (svg_content.empty() || svg_content.find("<svg") == std::string::npos) {
                Json::Value root;
                root["error"] = "Failed to generate SVG";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }

            // 返回原始SVG内容（不做任何修改）
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg_content);
            resp->setContentTypeCode(CT_TEXT_XML);
            resp->addHeader("Content-Type", "image/svg+xml");
            // 添加 Content-Disposition 让浏览器下载文件
            resp->addHeader("Content-Disposition", "attachment; filename=growth_profile.svg");
            callback(resp);
        },
        {Get}
    );

    // Symbol resolution endpoint (类似 brpc pprof 的 /pprof/symbol)
    // 使用 backward-cpp 进行符号化，支持内联函数
    app().registerHandler(
        "/pprof/symbol",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            // 只支持 POST 请求
            if (req->method() != Post) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k405MethodNotAllowed);
                resp->setBody("Method not allowed. Use POST.");
                callback(resp);
                return;
            }

            // 从请求体获取地址列表
            std::string bodyStr(req->body());
            std::istringstream iss(bodyStr);
            std::string address;
            std::ostringstream result;

            // 逐行读取地址并使用 backward-cpp 符号化
            while (std::getline(iss, address)) {
                if (address.empty() || address[0] == '#') {
                    continue;
                }

                // 移除 "0x" 前缀（如果有）
                std::string original_addr = address;
                if (address.size() > 2 && address[0] == '0' && address[1] == 'x') {
                    address = address.substr(2);
                }

                // 将十六进制地址转换为指针
                try {
                    uintptr_t addr = std::stoull(address, nullptr, 16);
                    void* ptr = reinterpret_cast<void*>(addr);

                    // 使用 backward-cpp 符号化（支持内联函数）
                    std::string symbol = profiler.resolveSymbolWithBackward(ptr);

                    // 返回格式: "原始地址 符号化结果"
                    result << original_addr << " " << symbol << "\n";
                } catch (const std::exception& e) {
                    // 转换失败，返回原始地址
                    result << original_addr << " " << original_addr << "\n";
                }
            }

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(result.str());
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Post}
    );

    // Standard pprof endpoint: /pprof/profile
    // Compatible with Go pprof tool
    app().registerHandler(
        "/pprof/profile",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            // 获取 seconds 参数，默认 30 秒
            int seconds = 30;
            auto seconds_param = req->getParameter("seconds");
            if (!seconds_param.empty()) {
                try {
                    seconds = std::stoi(seconds_param);
                    if (seconds < 1) seconds = 1;
                    if (seconds > 300) seconds = 300;  // 最多5分钟
                } catch (const std::exception& e) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k400BadRequest);
                    resp->setBody("Invalid seconds parameter");
                    callback(resp);
                    return;
                }
            }

            std::cout << "Received /pprof/profile request, seconds=" << seconds << std::endl;

            // 调用 getRawCPUProfile 获取原始 profile 数据
            std::string profile_data = profiler.getRawCPUProfile(seconds);

            if (profile_data.empty()) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k500InternalServerError);
                resp->setBody("Failed to generate CPU profile");
                callback(resp);
                return;
            }

            // 返回二进制 profile 数据
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(profile_data);
            resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
            resp->addHeader("Content-Disposition", "attachment; filename=profile");
            callback(resp);
        },
        {Get}
    );

    // Standard pprof endpoint: /pprof/heap
    // Compatible with Go pprof tool
    app().registerHandler(
        "/pprof/heap",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::cout << "Received /pprof/heap request" << std::endl;

            // 调用 getRawHeapSample 获取 heap sample 数据
            std::string heap_sample = profiler.getRawHeapSample();

            if (heap_sample.empty()) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k500InternalServerError);
                resp->setBody("Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER is set.");
                callback(resp);
                return;
            }

            // 返回 heap sample 数据（文本格式）
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(heap_sample);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->addHeader("Content-Disposition", "attachment; filename=heap");
            callback(resp);
        },
        {Get}
    );

    // Standard pprof endpoint: /pprof/growth
    // Compatible with Go pprof tool - returns heap growth stacks
    app().registerHandler(
        "/pprof/growth",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::cout << "Received /pprof/growth request" << std::endl;

            // 调用 getRawHeapGrowthStacks 获取 heap growth stacks 数据
            std::string heap_growth = profiler.getRawHeapGrowthStacks();

            if (heap_growth.empty()) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k500InternalServerError);
                resp->setBody("Failed to get heap growth stacks. No heap growth data available.");
                callback(resp);
                return;
            }

            // 返回 heap growth stacks 数据（文本格式）
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(heap_growth);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->addHeader("Content-Disposition", "attachment; filename=growth");
            callback(resp);
        },
        {Get}
    );
}

} // namespace profiler
