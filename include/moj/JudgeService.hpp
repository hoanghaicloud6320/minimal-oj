#pragma once

#include "moj/Contracts.hpp"
#include "moj/ProblemService.hpp"

#include <filesystem>
#include <string>

namespace moj {

class JudgeService {
public:
    explicit JudgeService(ProblemService& problemService);

    RefreshResult refreshProblem(const std::string& slug);
    JudgeResult judgeSubmission(const std::string& slug,
                                const std::string& participant,
                                const std::string& sourceCode);

private:
    ProblemService& problemService_;

    static int runCommand(const std::string& command);
    static int runExecutableWithRedirects(const std::filesystem::path& executable,
                                          const std::filesystem::path& input,
                                          const std::filesystem::path& output,
                                          const std::filesystem::path& error,
                                          int timeLimitMs,
                                          const std::string& argument = {},
                                          int testCount = 0);
    static std::string quote(const std::filesystem::path& path);
    static std::string quoteString(const std::string& value);
    static std::string powershellLiteral(const std::string& value);
    static std::vector<std::string> readTokens(const std::filesystem::path& path);
    static bool sameTokens(const std::filesystem::path& actual, const std::filesystem::path& expected);
    static std::string readTextIfExists(const std::filesystem::path& path);
    static std::string safeName(const std::string& value);
};

} // namespace moj

