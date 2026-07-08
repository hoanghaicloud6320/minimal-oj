#pragma once

#include "moj/Contracts.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace moj {

class ProblemRepository {
public:
    explicit ProblemRepository(std::filesystem::path dbPath);
    ~ProblemRepository();

    ProblemRepository(const ProblemRepository&) = delete;
    ProblemRepository& operator=(const ProblemRepository&) = delete;

    void initialize();
    void upsertProblem(const std::string& slug, const std::string& title);
    void deleteProblem(const std::string& slug);
    std::vector<ProblemSummary> listProblems() const;
    std::optional<ProblemSummary> findProblem(const std::string& slug) const;
    long long recordSubmission(const SubmissionSummary& submission);
    std::vector<SubmissionSummary> listRecentSubmissions(int limit) const;

private:
    sqlite3* db_ = nullptr;

    void exec(const std::string& sql) const;
};

} // namespace moj
