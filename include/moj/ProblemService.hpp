#pragma once

#include "moj/Contracts.hpp"
#include "moj/ProblemRepository.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace moj {

class ProblemService {
public:
    ProblemService(ProblemRepository& repository, std::filesystem::path problemsDir);

    std::vector<ProblemSummary> listProblems() const;
    ProblemBundle getProblem(const std::string& slug) const;
    ProblemBundle saveProblem(const ProblemBundle& bundle);
    void deleteProblem(const std::string& slug);
    std::filesystem::path problemDir(const std::string& slug) const;
    long long recordSubmission(const SubmissionSummary& submission);
    std::vector<SubmissionSummary> listRecentSubmissions(int limit) const;

private:
    ProblemRepository& repository_;
    std::filesystem::path problemsDir_;

    static void validateSlug(const std::string& slug);
};

} // namespace moj
