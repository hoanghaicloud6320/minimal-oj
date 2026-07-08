#include "moj/JudgeService.hpp"
#include "moj/ProblemRepository.hpp"
#include "moj/ProblemService.hpp"
#include "moj/Server.hpp"

#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>

int main() {
    try {
        moj::RuntimePaths paths;
        std::filesystem::create_directories(paths.dataDir);
        std::filesystem::create_directories(paths.problemsDir);

        moj::ProblemRepository repository(paths.dbPath);
        repository.initialize();
        moj::ProblemService problemService(repository, paths.problemsDir);
        moj::JudgeService judgeService(problemService);
        moj::Server::configure(problemService, judgeService);
        moj::Server::registerRoutes();

        drogon::app()
            .addListener("0.0.0.0", 3335)
            .setThreadNum(2)
            .run();
    } catch (const std::exception& error) {
        std::cerr << "minimal-oj failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
