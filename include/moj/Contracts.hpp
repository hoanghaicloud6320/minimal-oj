#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace moj {

struct ProblemConfig {
    int testCount = 5;
    int timeLimitMs = 1000;
    int memoryLimitMb = 256;
};

struct ProblemSummary {
    std::string slug;
    std::string title;
    std::string updatedAt;
};

struct ProblemBundle {
    std::string slug;
    std::string title;
    ProblemConfig config;
    std::string configJson;
    std::string statementMd;
    std::string genTestCpp;
    std::string genAnswerCpp;
    std::string updatedAt;
};

struct CaseResult {
    int index = 0;
    bool accepted = false;
    int exitCode = 0;
    std::string message;
};

struct JudgeResult {
    bool compiled = false;
    int passed = 0;
    int total = 0;
    std::string compileLog;
    std::vector<CaseResult> cases;
};

struct RefreshResult {
    bool ok = false;
    int generated = 0;
    std::string log;
};

struct RuntimePaths {
    std::filesystem::path dataDir = "data";
    std::filesystem::path problemsDir = "data/problems";
    std::filesystem::path dbPath = "data/oj.sqlite3";
    std::filesystem::path publicDir = "public";
};

} // namespace moj
