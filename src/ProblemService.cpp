#include "moj/ProblemService.hpp"

#include "moj/JsonUtil.hpp"

#include <fstream>
#include <regex>
#include <stdexcept>

namespace moj {

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write " + path.string());
    }
    out << text;
}

} // namespace

ProblemService::ProblemService(ProblemRepository& repository, std::filesystem::path problemsDir)
    : repository_(repository), problemsDir_(std::move(problemsDir)) {
    std::filesystem::create_directories(problemsDir_);
}

std::vector<ProblemSummary> ProblemService::listProblems() const {
    return repository_.listProblems();
}

ProblemBundle ProblemService::getProblem(const std::string& slug) const {
    validateSlug(slug);
    auto metadata = repository_.findProblem(slug);
    if (!metadata.has_value()) {
        throw std::runtime_error("problem not found");
    }
    auto dir = problemDir(slug);
    ProblemBundle bundle;
    bundle.slug = metadata->slug;
    bundle.title = metadata->title;
    bundle.updatedAt = metadata->updatedAt;
    bundle.configJson = readText(dir / "config.json");
    bundle.config = parseConfigJson(bundle.configJson.empty() ? "{}" : bundle.configJson);
    bundle.statementMd = readText(dir / "statement.md");
    bundle.genTestCpp = readText(dir / "gentest.cpp");
    bundle.genAnswerCpp = readText(dir / "gen_answer_from_test.cpp");
    return bundle;
}

ProblemBundle ProblemService::saveProblem(const ProblemBundle& bundle) {
    validateSlug(bundle.slug);
    if (bundle.title.empty()) {
        throw std::runtime_error("title is required");
    }
    auto config = parseConfigJson(bundle.configJson.empty() ? jsonToString(configToJson(bundle.config)) : bundle.configJson);
    auto dir = problemDir(bundle.slug);
    std::filesystem::create_directories(dir);
    writeText(dir / "config.json", jsonToString(configToJson(config)));
    writeText(dir / "statement.md", bundle.statementMd);
    writeText(dir / "gentest.cpp", bundle.genTestCpp);
    writeText(dir / "gen_answer_from_test.cpp", bundle.genAnswerCpp);
    repository_.upsertProblem(bundle.slug, bundle.title);
    return getProblem(bundle.slug);
}

void ProblemService::deleteProblem(const std::string& slug) {
    validateSlug(slug);
    repository_.deleteProblem(slug);
    auto dir = problemDir(slug);
    if (std::filesystem::exists(dir)) {
        std::filesystem::remove_all(dir);
    }
}

std::filesystem::path ProblemService::problemDir(const std::string& slug) const {
    validateSlug(slug);
    return problemsDir_ / slug;
}

void ProblemService::validateSlug(const std::string& slug) {
    static const std::regex pattern("^[a-z0-9][a-z0-9_-]{0,63}$");
    if (!std::regex_match(slug, pattern)) {
        throw std::runtime_error("slug must match ^[a-z0-9][a-z0-9_-]{0,63}$");
    }
}

} // namespace moj
