#include "core/backend_registry.hpp"

#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <stdexcept>
#include <utility>

namespace core {
namespace {

void throw_on_error(int result, sqlite3* database, const char* context) {
    if (result != SQLITE_OK) {
        throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(database));
    }
}

void execute(sqlite3* database, const char* sql) {
    char* error_message = nullptr;
    const int result = sqlite3_exec(database, sql, nullptr, nullptr, &error_message);
    if (result != SQLITE_OK) {
        const std::string message =
            error_message != nullptr ? error_message : sqlite3_errmsg(database);
        sqlite3_free(error_message);
        throw std::runtime_error(message);
    }
}

void expect_done(int result, sqlite3* database, const char* context) {
    if (result != SQLITE_DONE) {
        throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(database));
    }
}

class Statement {
  public:
    Statement(sqlite3* database, const char* sql) {
        throw_on_error(sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr), database,
                       "prepare statement");
    }
    ~Statement() {
        sqlite3_finalize(statement_);
    }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    sqlite3_stmt* get() const {
        return statement_;
    }

  private:
    sqlite3_stmt* statement_ = nullptr;
};

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error("failed to bind SQLite text parameter");
    }
}

} // namespace

class BackendRegistry::Impl {
  public:
    explicit Impl(const std::string& database_path) {
        const int result = sqlite3_open_v2(
            database_path.c_str(), &database,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
        if (result != SQLITE_OK) {
            const std::string message = database != nullptr ? sqlite3_errmsg(database)
                                                            : "failed to allocate SQLite database";
            sqlite3_close(database);
            throw std::runtime_error(message);
        }

        execute(database, "PRAGMA foreign_keys = ON;");
        execute(database, "CREATE TABLE IF NOT EXISTS backends ("
                          "id INTEGER PRIMARY KEY, host TEXT NOT NULL UNIQUE, "
                          "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
                          "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);");
        execute(database, "CREATE TABLE IF NOT EXISTS backend_urls ("
                          "id INTEGER PRIMARY KEY, backend_id INTEGER NOT NULL, url TEXT NOT NULL, "
                          "FOREIGN KEY (backend_id) REFERENCES backends(id) ON DELETE CASCADE, "
                          "UNIQUE (backend_id, url));");
    }
    ~Impl() {
        sqlite3_close(database);
    }
    sqlite3* database = nullptr;
};

BackendRegistry::BackendRegistry(std::string database_path)
    : impl_(std::make_unique<Impl>(database_path)) {
    spdlog::info("Backend registry opened at {}", database_path);
}

BackendRegistry::~BackendRegistry() = default;
BackendRegistry::BackendRegistry(BackendRegistry&&) noexcept = default;
BackendRegistry& BackendRegistry::operator=(BackendRegistry&&) noexcept = default;

bool BackendRegistry::register_backend(Backend backend) {
    if (backend.host.empty()) {
        throw std::invalid_argument("backend host must not be empty");
    }
    if (backend.urls.empty()) {
        throw std::invalid_argument("backend must register at least one URL");
    }
    for (const auto& url : backend.urls) {
        if (url.empty() || url.front() != '/') {
            throw std::invalid_argument("backend URLs must start with '/'");
        }
    }

    auto* database = impl_->database;
    bool created = false;
    try {
        Statement exists{database, "SELECT 1 FROM backends WHERE host = ?;"};
        bind_text(exists.get(), 1, backend.host);
        const int exists_result = sqlite3_step(exists.get());
        if (exists_result != SQLITE_ROW && exists_result != SQLITE_DONE) {
            throw std::runtime_error(std::string("check backend existence: ") +
                                     sqlite3_errmsg(database));
        }
        created = exists_result == SQLITE_DONE;

        execute(database, "BEGIN IMMEDIATE;");
        Statement upsert{database,
                         "INSERT INTO backends(host, updated_at) VALUES(?, CURRENT_TIMESTAMP) "
                         "ON CONFLICT(host) DO UPDATE SET updated_at = CURRENT_TIMESTAMP;"};
        bind_text(upsert.get(), 1, backend.host);
        expect_done(sqlite3_step(upsert.get()), database, "upsert backend");

        Statement find_id{database, "SELECT id FROM backends WHERE host = ?;"};
        bind_text(find_id.get(), 1, backend.host);
        if (sqlite3_step(find_id.get()) != SQLITE_ROW) {
            throw std::runtime_error("registered backend could not be found");
        }
        const auto backend_id = sqlite3_column_int64(find_id.get(), 0);

        Statement remove_urls{database, "DELETE FROM backend_urls WHERE backend_id = ?;"};
        throw_on_error(sqlite3_bind_int64(remove_urls.get(), 1, backend_id), database,
                       "bind backend ID");
        expect_done(sqlite3_step(remove_urls.get()), database, "remove backend URLs");

        Statement insert_url{database, "INSERT INTO backend_urls(backend_id, url) VALUES(?, ?);"};
        for (const auto& url : backend.urls) {
            sqlite3_reset(insert_url.get());
            sqlite3_clear_bindings(insert_url.get());
            throw_on_error(sqlite3_bind_int64(insert_url.get(), 1, backend_id), database,
                           "bind backend ID");
            bind_text(insert_url.get(), 2, url);
            expect_done(sqlite3_step(insert_url.get()), database, "insert backend URL");
        }
        execute(database, "COMMIT;");
    } catch (...) {
        sqlite3_exec(database, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }

    spdlog::info("Backend {} with {} URL(s)", created ? "registered" : "updated",
                 backend.urls.size());
    return created;
}

std::vector<Backend> BackendRegistry::backends() const {
    std::vector<Backend> result;
    Statement query{impl_->database,
                    "SELECT backends.host, backend_urls.url FROM backends "
                    "LEFT JOIN backend_urls ON backend_urls.backend_id = backends.id "
                    "ORDER BY backends.host, backend_urls.url;"};
    std::string current_host;
    for (;;) {
        const int step = sqlite3_step(query.get());
        if (step == SQLITE_DONE)
            break;
        throw_on_error(step, impl_->database, "read backends");
        const auto* host = reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0));
        if (current_host != host) {
            current_host = host;
            result.push_back({current_host, {}});
        }
        if (sqlite3_column_type(query.get(), 1) != SQLITE_NULL) {
            result.back().urls.emplace_back(
                reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 1)));
        }
    }
    return result;
}

} // namespace core
