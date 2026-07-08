#pragma once

#include "moj/Contracts.hpp"

#include <json/json.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace moj {

inline std::string jsonToString(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

inline Json::Value parseJson(const std::string& body) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errors;
    std::istringstream in(body);
    if (!Json::parseFromStream(builder, in, &root, &errors)) {
        throw std::runtime_error(errors);
    }
    return root;
}

inline ProblemConfig parseConfigJson(const std::string& text) {
    auto root = parseJson(text);
    ProblemConfig config;
    config.testCount = root.get("test_count", config.testCount).asInt();
    config.timeLimitMs = root.get("time_limit_ms", config.timeLimitMs).asInt();
    config.memoryLimitMb = root.get("memory_limit_mb", config.memoryLimitMb).asInt();
    if (config.testCount < 1) {
        throw std::runtime_error("test_count must be at least 1");
    }
    if (config.timeLimitMs < 1) {
        throw std::runtime_error("time_limit_ms must be positive");
    }
    if (config.memoryLimitMb < 1) {
        throw std::runtime_error("memory_limit_mb must be positive");
    }
    return config;
}

inline Json::Value configToJson(const ProblemConfig& config) {
    Json::Value root;
    root["test_count"] = config.testCount;
    root["time_limit_ms"] = config.timeLimitMs;
    root["memory_limit_mb"] = config.memoryLimitMb;
    return root;
}

inline Json::Value problemSummaryToJson(const ProblemSummary& problem) {
    Json::Value root;
    root["slug"] = problem.slug;
    root["title"] = problem.title;
    root["updated_at"] = problem.updatedAt;
    return root;
}

inline Json::Value problemBundleToJson(const ProblemBundle& problem) {
    Json::Value root;
    root["slug"] = problem.slug;
    root["title"] = problem.title;
    root["config"] = configToJson(problem.config);
    root["config_json"] = problem.configJson;
    root["statement_md"] = problem.statementMd;
    root["gentest_cpp"] = problem.genTestCpp;
    root["gen_answer_from_test_cpp"] = problem.genAnswerCpp;
    root["updated_at"] = problem.updatedAt;
    return root;
}

inline Json::Value judgeResultToJson(const JudgeResult& result) {
    Json::Value root;
    root["compiled"] = result.compiled;
    root["passed"] = result.passed;
    root["total"] = result.total;
    root["compile_log"] = result.compileLog;
    Json::Value cases(Json::arrayValue);
    for (const auto& item : result.cases) {
        Json::Value row;
        row["index"] = item.index;
        row["accepted"] = item.accepted;
        row["exit_code"] = item.exitCode;
        row["message"] = item.message;
        cases.append(row);
    }
    root["cases"] = cases;
    return root;
}

inline Json::Value refreshResultToJson(const RefreshResult& result) {
    Json::Value root;
    root["ok"] = result.ok;
    root["generated"] = result.generated;
    root["log"] = result.log;
    return root;
}

} // namespace moj
