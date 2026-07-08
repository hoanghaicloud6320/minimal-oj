#include "moj/ProblemRepository.hpp"

#include <algorithm>
#include <stdexcept>

namespace moj {

ProblemRepository::ProblemRepository(std::filesystem::path dbPath) {
    std::filesystem::create_directories(dbPath.parent_path());
    if (sqlite3_open(dbPath.string().c_str(), &db_) != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("cannot open sqlite database: " + error);
    }
}

ProblemRepository::~ProblemRepository() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

void ProblemRepository::initialize() {
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS problems (
            slug TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            updated_at TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )sql");
    exec(R"sql(
        CREATE TABLE IF NOT EXISTS submissions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            problem_slug TEXT NOT NULL,
            participant TEXT NOT NULL,
            passed INTEGER NOT NULL,
            total INTEGER NOT NULL,
            verdict TEXT NOT NULL,
            result_json TEXT NOT NULL,
            submitted_at TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )sql");
    exec("CREATE INDEX IF NOT EXISTS idx_submissions_submitted_at ON submissions(submitted_at DESC, id DESC);");
}

void ProblemRepository::upsertProblem(const std::string& slug, const std::string& title) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"sql(
        INSERT INTO problems(slug, title, updated_at)
        VALUES (?, ?, datetime('now'))
        ON CONFLICT(slug) DO UPDATE SET
            title = excluded.title,
            updated_at = datetime('now');
    )sql";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(stmt);
}

void ProblemRepository::deleteProblem(const std::string& slug) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM submissions WHERE problem_slug = ?", -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(stmt);

    if (sqlite3_prepare_v2(db_, "DELETE FROM problems WHERE slug = ?", -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(stmt);
}

std::vector<ProblemSummary> ProblemRepository::listProblems() const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT slug, title, updated_at FROM problems ORDER BY updated_at DESC, slug ASC";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    std::vector<ProblemSummary> problems;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ProblemSummary item;
        item.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        problems.push_back(std::move(item));
    }
    sqlite3_finalize(stmt);
    return problems;
}

std::optional<ProblemSummary> ProblemRepository::findProblem(const std::string& slug) const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT slug, title, updated_at FROM problems WHERE slug = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<ProblemSummary> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ProblemSummary item;
        item.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        item.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        result = std::move(item);
    }
    sqlite3_finalize(stmt);
    return result;
}

long long ProblemRepository::recordSubmission(const SubmissionSummary& submission) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"sql(
        INSERT INTO submissions(problem_slug, participant, passed, total, verdict, result_json, submitted_at)
        VALUES (?, ?, ?, ?, ?, ?, datetime('now'));
    )sql";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, submission.problemSlug.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, submission.participant.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, submission.passed);
    sqlite3_bind_int(stmt, 4, submission.total);
    sqlite3_bind_text(stmt, 5, submission.verdict.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, submission.resultJson.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error(error);
    }
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db_);
}

std::vector<SubmissionSummary> ProblemRepository::listRecentSubmissions(int limit) const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"sql(
        SELECT id, problem_slug, participant, passed, total, verdict, submitted_at, result_json
        FROM submissions
        ORDER BY submitted_at DESC, id DESC
        LIMIT ?;
    )sql";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_int(stmt, 1, std::max(1, std::min(limit, 200)));

    std::vector<SubmissionSummary> submissions;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SubmissionSummary item;
        item.id = sqlite3_column_int64(stmt, 0);
        item.problemSlug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        item.participant = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        item.passed = sqlite3_column_int(stmt, 3);
        item.total = sqlite3_column_int(stmt, 4);
        item.verdict = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        item.submittedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        item.resultJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        submissions.push_back(std::move(item));
    }
    sqlite3_finalize(stmt);
    return submissions;
}

void ProblemRepository::exec(const std::string& sql) const {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error == nullptr ? "sqlite error" : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

} // namespace moj
