#ifndef ANALYSIS_DATABASE_H
#define ANALYSIS_DATABASE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

struct sqlite3;

namespace ReWizard {

    class Function;
    class BasicBlock;
    class ExtendedInstruction;
    class SymbolTable;
    class Module;

    struct FunctionRecord {
        int64_t id;
        uintptr_t address;
        uintptr_t size;
        std::string name;
        std::string type;
        std::string convention;
        bool hasOpaquePredicates;
        bool isHybridVerified;
        bool isTrampoline;
    };

    struct XrefRecord {
        int64_t id;
        uintptr_t fromAddr;
        uintptr_t toAddr;
        std::string type;
    };

    struct SymbolRecord {
        int64_t id;
        uintptr_t address;
        std::string name;
        std::string type;
        std::string sourceDll;
        int64_t ordinal;
    };

    class AnalysisDatabase {
    public:
        ~AnalysisDatabase();

        static std::unique_ptr<AnalysisDatabase> Open(const std::string& path);

        bool IsOpen() const;
        void Close();

        bool CreateSchema();

        bool BeginTransaction();
        bool Commit();
        bool Rollback();

        bool SaveFunction(const Function& fn);
        bool SaveFunctions(const std::vector<std::unique_ptr<Function>>& fns);

        std::vector<FunctionRecord> QueryFunctions();
        std::optional<FunctionRecord> QueryFunctionByAddress(uintptr_t address);

        bool SaveBasicBlock(const BasicBlock& bb, int64_t functionId);
        bool SaveBasicBlocks(const std::vector<std::unique_ptr<BasicBlock>>& bbs, int64_t functionId);

        bool SaveInstruction(const ExtendedInstruction& insn, int64_t functionId, int64_t blockId);

        bool SaveSymbolTable(const SymbolTable& st);
        std::vector<SymbolRecord> QuerySymbolsByAddress(uintptr_t address);

        bool SaveXref(uintptr_t from, uintptr_t to, const std::string& type);
        std::vector<XrefRecord> QueryXrefsTo(uintptr_t address);
        std::vector<XrefRecord> QueryXrefsFrom(uintptr_t address);
        std::vector<XrefRecord> QueryAllXrefs();

        bool SaveModule(Module& module);

    private:
        AnalysisDatabase(sqlite3* db);

        sqlite3* m_db{ nullptr };
    };

}

#endif
