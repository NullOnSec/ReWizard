#include <ReWizard/Database/AnalysisDatabase.h>
#include <ReWizard/Analysis/Units/Function.h>
#include <ReWizard/Analysis/Units/BasicBlock.h>
#include <ReWizard/Analysis/Units/SymbolTable.h>
#include <ReWizard/Analysis/Units/Module.h>
#include <ReWizard/Disassembler/Disassembler.h>
#include <spdlog/spdlog.h>

#include <sqlite3.h>
#include <sstream>

namespace ReWizard {

    AnalysisDatabase::AnalysisDatabase(sqlite3* db) : m_db(db) {}

    AnalysisDatabase::~AnalysisDatabase() {
        Close();
    }

    std::unique_ptr<AnalysisDatabase> AnalysisDatabase::Open(const std::string& path) {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(path.c_str(), &db);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to open database '{}': {}", path, sqlite3_errmsg(db));
            if (db) sqlite3_close(db);
            return nullptr;
        }
        return std::unique_ptr<AnalysisDatabase>(new AnalysisDatabase(db));
    }

    bool AnalysisDatabase::IsOpen() const {
        return m_db != nullptr;
    }

    void AnalysisDatabase::Close() {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
    }

    bool AnalysisDatabase::CreateSchema() {
        if (!m_db) return false;

        const char* schema = R"sql(
            CREATE TABLE IF NOT EXISTS functions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                address INTEGER NOT NULL UNIQUE,
                size INTEGER NOT NULL DEFAULT 0,
                name TEXT NOT NULL,
                type TEXT,
                convention TEXT,
                has_opaque_predicates INTEGER NOT NULL DEFAULT 0,
                is_hybrid_verified INTEGER NOT NULL DEFAULT 0,
                is_trampoline INTEGER NOT NULL DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_functions_address ON functions(address);
            CREATE INDEX IF NOT EXISTS idx_functions_name ON functions(name);

            CREATE TABLE IF NOT EXISTS basic_blocks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                function_id INTEGER NOT NULL,
                start_addr INTEGER NOT NULL,
                end_addr INTEGER NOT NULL,
                name TEXT NOT NULL,
                FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_bb_start ON basic_blocks(start_addr);
            CREATE INDEX IF NOT EXISTS idx_bb_function ON basic_blocks(function_id);

            CREATE TABLE IF NOT EXISTS bb_successors (
                block_id INTEGER NOT NULL,
                successor_addr INTEGER NOT NULL,
                PRIMARY KEY (block_id, successor_addr),
                FOREIGN KEY (block_id) REFERENCES basic_blocks(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS bb_predecessors (
                block_id INTEGER NOT NULL,
                predecessor_addr INTEGER NOT NULL,
                PRIMARY KEY (block_id, predecessor_addr),
                FOREIGN KEY (block_id) REFERENCES basic_blocks(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS instructions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                address INTEGER NOT NULL UNIQUE,
                function_id INTEGER,
                block_id INTEGER,
                bytes BLOB,
                mnemonic TEXT,
                operands TEXT,
                size INTEGER NOT NULL DEFAULT 0,
                FOREIGN KEY (function_id) REFERENCES functions(id) ON DELETE SET NULL,
                FOREIGN KEY (block_id) REFERENCES basic_blocks(id) ON DELETE SET NULL
            );
            CREATE INDEX IF NOT EXISTS idx_insn_addr ON instructions(address);
            CREATE INDEX IF NOT EXISTS idx_insn_function ON instructions(function_id);
            CREATE INDEX IF NOT EXISTS idx_insn_block ON instructions(block_id);

            CREATE TABLE IF NOT EXISTS symbols (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                address INTEGER NOT NULL,
                name TEXT NOT NULL,
                type TEXT NOT NULL,
                source_dll TEXT,
                ordinal INTEGER NOT NULL DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_sym_address ON symbols(address);
            CREATE INDEX IF NOT EXISTS idx_sym_name ON symbols(name);

            CREATE TABLE IF NOT EXISTS xrefs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                from_addr INTEGER NOT NULL,
                to_addr INTEGER NOT NULL,
                type TEXT NOT NULL,
                UNIQUE(from_addr, to_addr, type)
            );
            CREATE INDEX IF NOT EXISTS idx_xref_to ON xrefs(to_addr);
            CREATE INDEX IF NOT EXISTS idx_xref_from ON xrefs(from_addr);
        )sql";

        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to create schema: {}", errMsg ? errMsg : "unknown error");
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    bool AnalysisDatabase::BeginTransaction() {
        if (!m_db) return false;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to begin transaction: {}", errMsg ? errMsg : "unknown error");
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    bool AnalysisDatabase::Commit() {
        if (!m_db) return false;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to commit transaction: {}", errMsg ? errMsg : "unknown error");
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    bool AnalysisDatabase::Rollback() {
        if (!m_db) return false;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to rollback transaction: {}", errMsg ? errMsg : "unknown error");
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

    bool AnalysisDatabase::SaveFunction(const Function& fn) {
        if (!m_db) return false;

        const char* sql = R"sql(
            INSERT INTO functions (address, size, name, type, convention, has_opaque_predicates, is_hybrid_verified, is_trampoline)
            VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
            ON CONFLICT(address) DO UPDATE SET
                size=excluded.size,
                name=excluded.name,
                type=excluded.type,
                convention=excluded.convention,
                has_opaque_predicates=excluded.has_opaque_predicates,
                is_hybrid_verified=excluded.is_hybrid_verified,
                is_trampoline=excluded.is_trampoline;
        )sql";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to prepare SaveFunction: {}", sqlite3_errmsg(m_db));
            return false;
        }

        uintptr_t start = fn.GetStart();
        uintptr_t end = fn.GetEnd();
        uintptr_t size = (end > start) ? (end - start) : 0;

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(start));
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(size));
        sqlite3_bind_text(stmt, 3, fn.GetName().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, "function", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, "", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, fn.HasOpaquePredicates() ? 1 : 0);
        sqlite3_bind_int(stmt, 7, fn.IsHybridVerified() ? 1 : 0);
        sqlite3_bind_int(stmt, 8, fn.IsTrampoline() ? 1 : 0);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            spdlog::error("Failed to save function {}: {}", fn.GetName(), sqlite3_errmsg(m_db));
            return false;
        }
        return true;
    }

    bool AnalysisDatabase::SaveFunctions(const std::vector<std::unique_ptr<Function>>& fns) {
        if (!BeginTransaction()) return false;
        bool ok = true;
        for (const auto& fn : fns) {
            if (fn && !SaveFunction(*fn)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return Commit();
        } else {
            Rollback();
            return false;
        }
    }

    std::vector<FunctionRecord> AnalysisDatabase::QueryFunctions() {
        std::vector<FunctionRecord> results;
        if (!m_db) return results;

        const char* sql = "SELECT id, address, size, name, type, convention, has_opaque_predicates, is_hybrid_verified, is_trampoline FROM functions ORDER BY address;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return results;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FunctionRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.address = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 1));
            rec.size = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 2));
            rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            rec.convention = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            rec.hasOpaquePredicates = sqlite3_column_int(stmt, 6) != 0;
            rec.isHybridVerified = sqlite3_column_int(stmt, 7) != 0;
            rec.isTrampoline = sqlite3_column_int(stmt, 8) != 0;
            results.push_back(std::move(rec));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::optional<FunctionRecord> AnalysisDatabase::QueryFunctionByAddress(uintptr_t address) {
        if (!m_db) return std::nullopt;

        const char* sql = "SELECT id, address, size, name, type, convention, has_opaque_predicates, is_hybrid_verified, is_trampoline FROM functions WHERE address = ?1 LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return std::nullopt;

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(address));

        FunctionRecord rec{};
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.address = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 1));
            rec.size = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 2));
            rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            rec.convention = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            rec.hasOpaquePredicates = sqlite3_column_int(stmt, 6) != 0;
            rec.isHybridVerified = sqlite3_column_int(stmt, 7) != 0;
            rec.isTrampoline = sqlite3_column_int(stmt, 8) != 0;
            sqlite3_finalize(stmt);
            return rec;
        }
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    bool AnalysisDatabase::SaveBasicBlock(const BasicBlock& bb, int64_t functionId) {
        if (!m_db) return false;

        const char* sql = R"sql(
            INSERT INTO basic_blocks (function_id, start_addr, end_addr, name)
            VALUES (?1, ?2, ?3, ?4)
            ON CONFLICT DO NOTHING;
        )sql";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to prepare SaveBasicBlock: {}", sqlite3_errmsg(m_db));
            return false;
        }

        sqlite3_bind_int64(stmt, 1, functionId);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(bb.GetStart()));
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(bb.GetEnd()));
        sqlite3_bind_text(stmt, 4, bb.GetName().c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool AnalysisDatabase::SaveBasicBlocks(const std::vector<std::unique_ptr<BasicBlock>>& bbs, int64_t functionId) {
        if (!BeginTransaction()) return false;
        bool ok = true;
        for (const auto& bb : bbs) {
            if (bb && !SaveBasicBlock(*bb, functionId)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return Commit();
        } else {
            Rollback();
            return false;
        }
    }

    bool AnalysisDatabase::SaveInstruction(const ExtendedInstruction& insn, int64_t functionId, int64_t blockId) {
        if (!m_db) return false;
        // TODO: implement when ExtendedInstruction serialization is available
        return true;
    }

    bool AnalysisDatabase::SaveSymbolTable(const SymbolTable& st) {
        if (!m_db) return false;
        if (!BeginTransaction()) return false;

        const char* sql = R"sql(
            INSERT INTO symbols (address, name, type, source_dll, ordinal)
            VALUES (?1, ?2, ?3, ?4, ?5)
            ON CONFLICT DO NOTHING;
        )sql";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            Rollback();
            return false;
        }

        bool ok = true;
        for (const auto& imp : st.GetImports()) {
            sqlite3_reset(stmt);
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(imp.address));
            sqlite3_bind_text(stmt, 2, imp.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, "import", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, imp.dll.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 5, 0);
            if (sqlite3_step(stmt) != SQLITE_DONE) { ok = false; break; }
        }

        if (ok) {
            for (const auto& exp : st.GetExports()) {
                sqlite3_reset(stmt);
                sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(exp.address));
                sqlite3_bind_text(stmt, 2, exp.name.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, "export", -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, "", -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt, 5, exp.ordinal);
                if (sqlite3_step(stmt) != SQLITE_DONE) { ok = false; break; }
            }
        }

        sqlite3_finalize(stmt);
        if (ok) {
            return Commit();
        } else {
            Rollback();
            return false;
        }
    }

    std::vector<SymbolRecord> AnalysisDatabase::QuerySymbolsByAddress(uintptr_t address) {
        std::vector<SymbolRecord> results;
        if (!m_db) return results;

        const char* sql = "SELECT id, address, name, type, source_dll, ordinal FROM symbols WHERE address = ?1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return results;

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(address));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SymbolRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.address = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 1));
            rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            rec.sourceDll = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            rec.ordinal = sqlite3_column_int64(stmt, 5);
            results.push_back(std::move(rec));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    bool AnalysisDatabase::SaveXref(uintptr_t from, uintptr_t to, const std::string& type) {
        if (!m_db) return false;

        const char* sql = R"sql(
            INSERT INTO xrefs (from_addr, to_addr, type)
            VALUES (?1, ?2, ?3)
            ON CONFLICT DO NOTHING;
        )sql";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(from));
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(to));
        sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    std::vector<XrefRecord> AnalysisDatabase::QueryXrefsTo(uintptr_t address) {
        std::vector<XrefRecord> results;
        if (!m_db) return results;

        const char* sql = "SELECT id, from_addr, to_addr, type FROM xrefs WHERE to_addr = ?1 ORDER BY from_addr;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return results;

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(address));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            XrefRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.fromAddr = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 1));
            rec.toAddr = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 2));
            rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            results.push_back(std::move(rec));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<XrefRecord> AnalysisDatabase::QueryAllXrefs() {
        std::vector<XrefRecord> results;
        if (!m_db) return results;

        const char* sql = "SELECT id, from_addr, to_addr, type FROM xrefs ORDER BY from_addr, to_addr;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return results;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            XrefRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.fromAddr = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 1));
            rec.toAddr = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 2));
            rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            results.push_back(std::move(rec));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    std::vector<XrefRecord> AnalysisDatabase::QueryXrefsFrom(uintptr_t address) {
        std::vector<XrefRecord> results;
        if (!m_db) return results;

        const char* sql = "SELECT id, from_addr, to_addr, type FROM xrefs WHERE from_addr = ?1 ORDER BY to_addr;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return results;

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(address));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            XrefRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.fromAddr = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 1));
            rec.toAddr = static_cast<uintptr_t>(sqlite3_column_int64(stmt, 2));
            rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            results.push_back(std::move(rec));
        }
        sqlite3_finalize(stmt);
        return results;
    }

    bool AnalysisDatabase::SaveModule(Module& module) {
        if (!m_db) return false;

        if (!CreateSchema()) return false;
        if (!SaveFunctions(module.GetFunctions())) return false;

        auto funcs = QueryFunctions();
        std::map<uintptr_t, int64_t> funcIdByAddr;
        for (const auto& rec : funcs) {
            funcIdByAddr[rec.address] = rec.id;
        }

        if (!BeginTransaction()) return false;
        bool ok = true;

        for (const auto& fnPtr : module.GetFunctions()) {
            if (!fnPtr) continue;
            auto it = funcIdByAddr.find(fnPtr->GetStart());
            if (it == funcIdByAddr.end()) continue;
            int64_t funcId = it->second;

            for (const auto& bbPtr : fnPtr->GetBasicBlocks()) {
                if (!bbPtr) continue;
                if (!SaveBasicBlock(*bbPtr, funcId)) {
                    ok = false;
                    break;
                }
                for (const auto& succ : bbPtr->GetSuccessors()) {
                    // TODO: resolve block_id for successor and insert into bb_successors
                }
                for (const auto& pred : bbPtr->GetPredecessors()) {
                    // TODO: resolve block_id for predecessor and insert into bb_predecessors
                }
            }
            if (!ok) break;

            for (const auto& cs : fnPtr->GetCallSites()) {
                if (!SaveXref(cs.first, cs.second, "call")) {
                    ok = false;
                    break;
                }
            }
            if (!ok) break;
        }

        if (ok) {
            if (module.GetSymbolTable()) {
                if (!SaveSymbolTable(*module.GetSymbolTable())) {
                    ok = false;
                }
            }
        }

        if (ok) {
            return Commit();
        } else {
            Rollback();
            return false;
        }
    }

}
