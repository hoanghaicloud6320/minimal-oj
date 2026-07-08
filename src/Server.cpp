#include "moj/Server.hpp"

#include "moj/JsonUtil.hpp"

#include <fstream>
#include <stdexcept>

namespace moj {

ProblemService* Server::problemService_ = nullptr;
JudgeService* Server::judgeService_ = nullptr;

void Server::configure(ProblemService& problemService, JudgeService& judgeService) {
    problemService_ = &problemService;
    judgeService_ = &judgeService;
}

void Server::registerRoutes() {
    drogon::app().registerHandler("/", [](drogon::HttpRequestPtr req) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::index(std::move(req));
    }, {drogon::Get});
    drogon::app().registerHandler("/api/health", [](drogon::HttpRequestPtr req) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::health(std::move(req));
    }, {drogon::Get});
    drogon::app().registerHandler("/api/problems", [](drogon::HttpRequestPtr req) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::listProblems(std::move(req));
    }, {drogon::Get});
    drogon::app().registerHandler("/api/problems", [](drogon::HttpRequestPtr req) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::createProblem(std::move(req));
    }, {drogon::Post});
    drogon::app().registerHandler("/api/problems/{1}", [](drogon::HttpRequestPtr req, std::string slug) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::getProblem(std::move(req), std::move(slug));
    }, {drogon::Get});
    drogon::app().registerHandler("/api/problems/{1}", [](drogon::HttpRequestPtr req, std::string slug) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::updateProblem(std::move(req), std::move(slug));
    }, {drogon::Put});
    drogon::app().registerHandler("/api/problems/{1}", [](drogon::HttpRequestPtr req, std::string slug) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::deleteProblem(std::move(req), std::move(slug));
    }, {drogon::Delete});
    drogon::app().registerHandler("/api/problems/{1}/refresh", [](drogon::HttpRequestPtr req, std::string slug) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::refreshProblem(std::move(req), std::move(slug));
    }, {drogon::Post});
    drogon::app().registerHandler("/api/problems/{1}/submit", [](drogon::HttpRequestPtr req, std::string slug) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::submit(std::move(req), std::move(slug));
    }, {drogon::Post});
    drogon::app().registerHandler("/api/submissions/recent", [](drogon::HttpRequestPtr req) -> drogon::Task<drogon::HttpResponsePtr> {
        co_return co_await Server::listRecentSubmissions(std::move(req));
    }, {drogon::Get});
}

drogon::Task<drogon::HttpResponsePtr> Server::index(drogon::HttpRequestPtr) {
    std::ifstream in("public/index.html", std::ios::binary);
    if (!in) {
        co_return errorResponse("public/index.html not found", drogon::k500InternalServerError);
    }
    auto html = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setContentTypeCode(drogon::CT_TEXT_HTML);
    response->setBody(std::move(html));
    co_return response;
}

drogon::Task<drogon::HttpResponsePtr> Server::health(drogon::HttpRequestPtr) {
    Json::Value root;
    root["ok"] = true;
    root["service"] = "minimal-oj";
    co_return jsonResponse(root);
}

drogon::Task<drogon::HttpResponsePtr> Server::listProblems(drogon::HttpRequestPtr) {
    try {
        Json::Value items(Json::arrayValue);
        for (const auto& problem : problemService_->listProblems()) {
            items.append(problemSummaryToJson(problem));
        }
        Json::Value root;
        root["problems"] = items;
        co_return jsonResponse(root);
    } catch (const std::exception& error) {
        co_return errorResponse(error.what(), drogon::k500InternalServerError);
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::createProblem(drogon::HttpRequestPtr req) {
    try {
        auto root = parseJson(std::string(req->getBody()));
        auto saved = problemService_->saveProblem(bundleFromJson(root));
        co_return jsonResponse(problemBundleToJson(saved), drogon::k201Created);
    } catch (const std::exception& error) {
        co_return errorResponse(error.what());
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::getProblem(drogon::HttpRequestPtr, std::string slug) {
    try {
        co_return jsonResponse(problemBundleToJson(problemService_->getProblem(slug)));
    } catch (const std::exception& error) {
        co_return errorResponse(error.what(), drogon::k404NotFound);
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::updateProblem(drogon::HttpRequestPtr req, std::string slug) {
    try {
        auto root = parseJson(std::string(req->getBody()));
        auto saved = problemService_->saveProblem(bundleFromJson(root, slug));
        co_return jsonResponse(problemBundleToJson(saved));
    } catch (const std::exception& error) {
        co_return errorResponse(error.what());
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::deleteProblem(drogon::HttpRequestPtr, std::string slug) {
    try {
        problemService_->deleteProblem(slug);
        Json::Value root;
        root["ok"] = true;
        co_return jsonResponse(root);
    } catch (const std::exception& error) {
        co_return errorResponse(error.what());
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::refreshProblem(drogon::HttpRequestPtr, std::string slug) {
    try {
        co_return jsonResponse(refreshResultToJson(judgeService_->refreshProblem(slug)));
    } catch (const std::exception& error) {
        co_return errorResponse(error.what());
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::submit(drogon::HttpRequestPtr req, std::string slug) {
    try {
        auto root = parseJson(std::string(req->getBody()));
        auto participant = root.get("participant", "anonymous").asString();
        auto source = root.get("source_code", "").asString();
        if (source.empty()) {
            throw std::runtime_error("source_code is required");
        }
        auto result = judgeService_->judgeSubmission(slug, participant, source);
        auto resultJson = judgeResultToJson(result);
        SubmissionSummary submission;
        submission.problemSlug = slug;
        submission.participant = participant.empty() ? "anonymous" : participant;
        submission.passed = result.passed;
        submission.total = result.total;
        submission.verdict = verdictFor(result);
        submission.resultJson = jsonToString(resultJson);
        submission.sourceCode = source;
        submission.id = problemService_->recordSubmission(submission);
        resultJson["submission_id"] = Json::Int64(submission.id);
        resultJson["verdict"] = submission.verdict;
        co_return jsonResponse(resultJson);
    } catch (const std::exception& error) {
        co_return errorResponse(error.what());
    }
}

drogon::Task<drogon::HttpResponsePtr> Server::listRecentSubmissions(drogon::HttpRequestPtr req) {
    try {
        int limit = 25;
        auto limitText = req->getParameter("limit");
        if (!limitText.empty()) {
            limit = std::stoi(limitText);
        }
        Json::Value items(Json::arrayValue);
        for (const auto& submission : problemService_->listRecentSubmissions(limit)) {
            items.append(submissionSummaryToJson(submission));
        }
        Json::Value root;
        root["submissions"] = items;
        co_return jsonResponse(root);
    } catch (const std::exception& error) {
        co_return errorResponse(error.what());
    }
}

ProblemBundle Server::bundleFromJson(const Json::Value& root, const std::string& fallbackSlug) {
    ProblemBundle bundle;
    bundle.slug = root.get("slug", fallbackSlug).asString();
    bundle.title = root.get("title", "").asString();
    if (root.isMember("config_json")) {
        bundle.configJson = root["config_json"].asString();
    } else if (root.isMember("config")) {
        bundle.configJson = jsonToString(root["config"]);
    } else {
        bundle.configJson = jsonToString(configToJson(bundle.config));
    }
    bundle.config = parseConfigJson(bundle.configJson);
    bundle.statementMd = root.get("statement_md", "").asString();
    bundle.genTestCpp = root.get("gentest_cpp", "").asString();
    bundle.genAnswerCpp = root.get("gen_answer_from_test_cpp", "").asString();
    return bundle;
}

std::string Server::verdictFor(const JudgeResult& result) {
    if (!result.compiled) {
        return "CE";
    }
    if (result.total > 0 && result.passed == result.total) {
        return "AC";
    }
    for (const auto& item : result.cases) {
        if (item.exitCode == 124) {
            return "TLE";
        }
        if (item.exitCode != 0) {
            return "RTE";
        }
    }
    return "WA";
}

drogon::HttpResponsePtr Server::jsonResponse(const Json::Value& value, drogon::HttpStatusCode status) {
    auto response = drogon::HttpResponse::newHttpJsonResponse(value);
    response->setStatusCode(status);
    return response;
}

drogon::HttpResponsePtr Server::errorResponse(const std::string& message, drogon::HttpStatusCode status) {
    Json::Value root;
    root["ok"] = false;
    root["error"] = message;
    return jsonResponse(root, status);
}

} // namespace moj

