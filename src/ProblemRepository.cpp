#include "moj/ProblemRepository.hpp"

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

void ProblemRepository::exec(const std::string& sql) const {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error == nullptr ? "sqlite error" : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

} // namespace moj
