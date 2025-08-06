#ifndef FILE_LOADER_H
#define FILE_LOADER_H

#include <LIEF/Abstract.hpp>
#include <string>
#include <span>
#include <optional>
#include <memory>

namespace ReWizard {
    enum class FileLoaderStatus {
        Success,
        EmptyFile,
        InvalidSize,
        FileOpenError,
        LiefParserError,
        AllocationError,
        SectionLoaderError,
        OutOfBoundsError,

    };

    class FileLoader {
    public:
        static std::unique_ptr<FileLoader> Create(const std::string& name);
        ~FileLoader();

        bool Load(bool executable = false);

        static std::optional<size_t> GetSize(std::unique_ptr<LIEF::Binary>& t);
        static std::optional<size_t> GetHeaderSize(std::unique_ptr<LIEF::Binary>& t);

        FileLoaderStatus Status() { return m_status; }
        std::span<uint8_t> Raw() { return m_mappedSpan; }
        size_t MappedSize() { return m_mappedSize; };
        LIEF::Binary* Binary() { return m_target.get(); }

    private:
        FileLoader(const std::string& name);
        bool LoadSections();

    private:
        std::unique_ptr<LIEF::Binary>   m_target{ nullptr };
        std::vector<uint8_t>            m_raw{};
        std::span<uint8_t>              m_mappedSpan{};
        size_t                          m_mappedSize{ 0 };
        uint8_t*                        m_mappedPtr{ nullptr };
        FileLoaderStatus                m_status = { FileLoaderStatus::Success };
        
    };

    class FileLoaderProvider {
    };
}

#endif
