#include "moj/JudgeService.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

    // Create a unique build/test directory to allow concurrent refreshes
    auto buildDir = dir / ("build-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto testsDir = dir / ("tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(buildDir);
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
        std::filesystem::remove_all(buildDir);
        std::filesystem::remove_all(testsDir);
        return result;
    }

    for (int i = 1; i <= problem.config.testCount; ++i) {
        auto input = testsDir / numberName(i, ".in");
        auto answer = testsDir / numberName(i, ".ans");
        auto error = testsDir / numberName(i, ".err");
        auto empty = testsDir / "_empty.in";
        writeText(empty, "");
        int genCode = runExecutableWithRedirects(genExe, empty, input, error, problem.config.timeLimitMs, problem.config.memoryLimitMb, std::to_string(i), problem.config.testCount);
        if (genCode != 0) {
            result.log = "gentest failed on case " + std::to_string(i) + ": " + exitCodeMessage(genCode) + "\n" + readTextIfExists(error);
            return result;
        }
        int ansCode = runExecutableWithRedirects(ansExe, input, answer, error, problem.config.timeLimitMs, problem.config.memoryLimitMb, "", 0);
        if (ansCode != 0) {
            result.log = "gen_answer_from_test failed on case " + std::to_string(i) + ": " + exitCodeMessage(ansCode) + "\n" + readTextIfExists(error);
            std::filesystem::remove_all(buildDir);
            std::filesystem::remove_all(testsDir);
            return result;
        }
        ++result.generated;
    }

    // Atomically update the test directory
    auto finalTestsDir = dir / "tests";
    std::filesystem::remove_all(finalTestsDir);
    std::filesystem::rename(testsDir, finalTestsDir);
    std::filesystem::remove_all(buildDir);

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
    
    // Ensure cleanup of run directory
    struct RunDirGuard {
        std::filesystem::path p;
        RunDirGuard(std::filesystem::path path) : p(std::move(path)) {}
        ~RunDirGuard() {
            if (p.empty()) return;
            for (int i = 0; i < 5; ++i) {
                std::error_code ec;
                std::filesystem::remove_all(p, ec);
                if (!ec) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    } runDirGuard(runDir);

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

    // Check if tests exist. Since we now use a temporary directory for generating,
    // the final `testsDir` should be ready. However, we should be careful about
    // the race condition where `refreshProblem` might be deleting/renaming `testsDir`.
    // A simple check is to verify if the directory exists.
    if (!std::filesystem::exists(testsDir)) {
         // Fallback or error, this means no tests are ready yet.
         result.compileLog += "\nError: Testcases not generated yet. Please refresh problem.";
    return result;
}

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
        row.exitCode = runExecutableWithRedirects(exe, input, actual, error, problem.config.timeLimitMs, problem.config.memoryLimitMb, "", 0);
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
                                             int memoryLimitMb,
                                             const std::string& argument,
                                             int testCount) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES security;
    security.nLength = sizeof(SECURITY_ATTRIBUTES);
    security.lpSecurityDescriptor = nullptr;
    security.bInheritHandle = TRUE;

    HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
    if (!hJob) return 1;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    jeli.ProcessMemoryLimit = (SIZE_T)memoryLimitMb * 1024 * 1024;
    SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));

    HANDLE inputHandle = CreateFileW(input.wstring().c_str(),
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     &security,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
    HANDLE outputHandle = CreateFileW(output.wstring().c_str(),
                                      GENERIC_WRITE,
                                      FILE_SHARE_READ,
                                      &security,
                                      CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL,
                                      nullptr);
    HANDLE errorHandle = CreateFileW(error.wstring().c_str(),
                                     GENERIC_WRITE,
                                     FILE_SHARE_READ,
                                     &security,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
    if (inputHandle == INVALID_HANDLE_VALUE || outputHandle == INVALID_HANDLE_VALUE || errorHandle == INVALID_HANDLE_VALUE) {
        if (inputHandle != INVALID_HANDLE_VALUE) CloseHandle(inputHandle);
        if (outputHandle != INVALID_HANDLE_VALUE) CloseHandle(outputHandle);
        if (errorHandle != INVALID_HANDLE_VALUE) CloseHandle(errorHandle);
        CloseHandle(hJob);
        return 1;
    }

    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = inputHandle;
    startup.hStdOutput = outputHandle;
    startup.hStdError = errorHandle;

    PROCESS_INFORMATION process;
    ZeroMemory(&process, sizeof(process));

    std::string command = quote(executable);
    if (!argument.empty()) {
        command += " " + quoteString(argument);
        if (testCount > 0) {
            command += " " + std::to_string(testCount);
        }
    }
    std::wstring wideCommand(command.begin(), command.end());
    std::vector<wchar_t> mutableCommand(wideCommand.begin(), wideCommand.end());
    mutableCommand.push_back(L'\0');

    BOOL created = CreateProcessW(nullptr,
                                  mutableCommand.data(),
                                  nullptr,
                                  nullptr,
                                  TRUE,
                                  CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB,
                                  nullptr,
                                  nullptr,
                                  &startup,
                                  &process);
    
    CloseHandle(inputHandle);
    CloseHandle(outputHandle);
    CloseHandle(errorHandle);

    if (created) {
        AssignProcessToJobObject(hJob, process.hProcess);
    }
    
    if (!created) {
        CloseHandle(hJob);
        return 1;
    }

    DWORD waitCode = WaitForSingleObject(process.hProcess, static_cast<DWORD>(timeLimitMs));
    DWORD exitCode = 1;
    if (waitCode == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 124);
        WaitForSingleObject(process.hProcess, INFINITE);
        exitCode = 124;
    } else {
        GetExitCodeProcess(process.hProcess, &exitCode);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(hJob);
    return static_cast<int>(exitCode);
#else
    // TODO: Implement memory limit for non-Windows (e.g. setrlimit)
    std::string command = "timeout " + std::to_string((timeLimitMs + 999) / 1000) + "s " + quote(executable);
    if (!argument.empty()) {
        command += " " + quoteString(argument);
        if (testCount > 0) {
            command += " " + std::to_string(testCount);
        }
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

std::string JudgeService::exitCodeMessage(int code) {
    if (code == 124) return "TLE (Time Limit Exceeded)";
    return "RTE (Runtime Error) - Exit Code: " + std::to_string(code);
}

} // namespace moj

