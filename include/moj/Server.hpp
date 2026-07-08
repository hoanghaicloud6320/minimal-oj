#pragma once

#include "moj/JudgeService.hpp"
#include "moj/ProblemService.hpp"

#include <drogon/drogon.h>

namespace moj {

class Server final {
public:
    static void configure(ProblemService& problemService, JudgeService& judgeService);
    static void registerRoutes();

    static drogon::Task<drogon::HttpResponsePtr> index(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> health(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> listProblems(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> createProblem(drogon::HttpRequestPtr req);
    static drogon::Task<drogon::HttpResponsePtr> getProblem(drogon::HttpRequestPtr req, std::string slug);
    static drogon::Task<drogon::HttpResponsePtr> updateProblem(drogon::HttpRequestPtr req, std::string slug);
    static drogon::Task<drogon::HttpResponsePtr> deleteProblem(drogon::HttpRequestPtr req, std::string slug);
    static drogon::Task<drogon::HttpResponsePtr> refreshProblem(drogon::HttpRequestPtr req, std::string slug);
    static drogon::Task<drogon::HttpResponsePtr> submit(drogon::HttpRequestPtr req, std::string slug);

private:
    static ProblemService* problemService_;
    static JudgeService* judgeService_;

    static ProblemBundle bundleFromJson(const Json::Value& root, const std::string& fallbackSlug = {});
    static drogon::HttpResponsePtr jsonResponse(const Json::Value& value,
                                                drogon::HttpStatusCode status = drogon::k200OK);
    static drogon::HttpResponsePtr errorResponse(const std::string& message,
                                                 drogon::HttpStatusCode status = drogon::k400BadRequest);
};

} // namespace moj
