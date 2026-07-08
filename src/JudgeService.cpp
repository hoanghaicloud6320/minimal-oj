#include "moj/JudgeService.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace moj {

namespace {

std::string exeSuffix() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

std::filesystem::path uniqueRunDir(const std::filesystem::path& base, const std::string& prefix) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = base / "runs" / (prefix + "-" + std::to_string(now));
    std::filesystem::create_directories(dir);
    return dir;
}

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write " + path.string());
    }
    out << text;
}

std::string numberName(int value, const std::string& extension) {
    std::ostringstream out;
    out.width(3);
    out.fill('0');
    out << value << extension;
    return out.str();
}

} // namespace

JudgeService::JudgeService(ProblemService& problemService) : problemService_(problemService) {}

RefreshResult JudgeService::refreshProblem(const std::string& slug) {
    auto problem = problemService_.getProblem(slug);
    auto dir = problemService_.problemDir(slug);
    auto buildDir = dir / "build";
    auto testsDir = dir / "tests";
    std::filesystem::create_directories(buildDir);
    std::filesystem::remove_all(testsDir);
    std::filesystem::create_directories(testsDir);

    auto genExe = buildDir / ("gentest" + exeSuffix());
    auto ansExe = buildDir / ("gen_answer_from_test" + exeSuffix());
    auto genLog = buildDir / "gentest.compile.log";
    auto ansLog = buildDir / "answer.compile.log";

    std::string compileGen = "g++ -std=c++20 -O2 " + quote(dir / "gentest.cpp") + " -o " + quote(genExe) + " 2> " + quote(genLog);
    std::string compileAns = "g++ -std=c++20 -O2 " + quote(dir / "gen_answer_from_test.cpp") + " -o " + quote(ansExe) + " 2> " + quote(ansLog);

    RefreshResult result;
    if (runCommand(compileGen) != 0) {
        result.log = readTextIfExists(genLog);
        return result;
    }
    if (runCommand(compileAns) != 0) {
        result.log = readTextIfExists(ansLog);
        return result;
    }

    for (int i = 1; i <= problem.config.testCount; ++i) {
        auto input = testsDir / numberName(i, ".in");
        auto answer = testsDir / numberName(i, ".ans");
        auto error = testsDir / numberName(i, ".err");
        auto empty = testsDir / "_empty.in";
        writeText(empty, "");
        int genCode = runExecutableWithRedirects(genExe, empty, input, error, problem.config.timeLimitMs, std::to_string(i));
        if (genCode != 0) {
            result.log = "gentest failed on case " + std::to_string(i) + "\n" + readTextIfExists(error);
            return result;
        }
        int ansCode = runExecutableWithRedirects(ansExe, input, answer, error, problem.config.timeLimitMs);
        if (ansCode != 0) {
            result.log = "gen_answer_from_test failed on case " + std::to_string(i) + "\n" + readTextIfExists(error);
            return result;
        }
        ++result.generated;
    }
    result.ok = true;
    result.log = "generated " + std::to_string(result.generated) + " testcase(s)";
    return result;
}

JudgeResult JudgeService::judgeSubmission(const std::string& slug,
                                          const std::string& participant,
                                          const std::string& sourceCode) {
    auto problem = problemService_.getProblem(slug);
    auto dir = problemService_.problemDir(slug);
    auto runDir = uniqueRunDir(dir, safeName(participant.empty() ? "anonymous" : participant));
    auto source = runDir / "main.cpp";
    auto exe = runDir / ("main" + exeSuffix());
    auto compileLog = runDir / "compile.log";
    writeText(source, sourceCode);

    JudgeResult result;
    result.total = problem.config.testCount;
    std::string compile = "g++ -std=c++20 -O2 " + quote(source) + " -o " + quote(exe) + " 2> " + quote(compileLog);
    int compileCode = runCommand(compile);
    result.compileLog = readTextIfExists(compileLog);
    result.compiled = compileCode == 0;
    if (!result.compiled) {
        return result;
    }

    auto testsDir = dir / "tests";
    for (int i = 1; i <= problem.config.testCount; ++i) {
        auto input = testsDir / numberName(i, ".in");
        auto expected = testsDir / numberName(i, ".ans");
        auto actual = runDir / numberName(i, ".out");
        auto error = runDir / numberName(i, ".err");
        CaseResult row;
        row.index = i;
        if (!std::filesystem::exists(input) || !std::filesystem::exists(expected)) {
            row.message = "missing testcase; refresh problem first";
            result.cases.push_back(row);
            continue;
        }
        row.exitCode = runExecutableWithRedirects(exe, input, actual, error, problem.config.timeLimitMs);
        if (row.exitCode == 124) {
            row.message = "time limit exceeded";
        } else if (row.exitCode != 0) {
            row.message = "runtime error: " + readTextIfExists(error);
        } else if (sameTokens(actual, expected)) {
            row.accepted = true;
            row.message = "accepted";
            ++result.passed;
        } else {
            row.message = "wrong answer";
        }
        result.cases.push_back(std::move(row));
    }
    return result;
}

int JudgeService::runCommand(const std::string& command) {
    int code = std::system(command.c_str());
    return code;
}

int JudgeService::runExecutableWithRedirects(const std::filesystem::path& executable,
                                             const std::filesystem::path& input,
                                             const std::filesystem::path& output,
                                             const std::filesystem::path& error,
                                             int timeLimitMs,
                                             const std::string& argument) {
#ifdef _WIN32
    (void)timeLimitMs;
    std::string command = quote(executable);
    if (!argument.empty()) {
        command += " " + quoteString(argument);
    }
    command += " < " + quote(input) + " > " + quote(output) + " 2> " + quote(error);
    return runCommand("cmd /S /C \"" + command + "\"");
#else
    std::string command = "timeout " + std::to_string((timeLimitMs + 999) / 1000) + "s " + quote(executable);
    if (!argument.empty()) {
        command += " " + quoteString(argument);
    }
    command += " < " + quote(input) + " > " + quote(output) + " 2> " + quote(error);
    return runCommand(command);
#endif
}

std::string JudgeService::quote(const std::filesystem::path& path) {
    return quoteString(path.string());
}

std::string JudgeService::quoteString(const std::string& value) {
#ifdef _WIN32
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\\\"";
        } else {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
#else
    std::string escaped = "'";
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped += ch;
        }
    }
    escaped += "'";
    return escaped;
#endif
}

std::string JudgeService::powershellLiteral(const std::string& value) {
    std::string escaped = "'";
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "''";
        } else {
            escaped += ch;
        }
    }
    escaped += "'";
    return escaped;
}

std::vector<std::string> JudgeService::readTokens(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<std::string> tokens;
    std::string token;
    while (in >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool JudgeService::sameTokens(const std::filesystem::path& actual, const std::filesystem::path& expected) {
    return readTokens(actual) == readTokens(expected);
}

std::string JudgeService::readTextIfExists(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string JudgeService::safeName(const std::string& value) {
    std::string out;
    for (char ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
            out.push_back(ch);
        }
    }
    return out.empty() ? "anonymous" : out;
}

} // namespace moj
