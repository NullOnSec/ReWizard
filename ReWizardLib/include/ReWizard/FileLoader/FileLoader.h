#ifndef FILE_LOADER_H
#define FILE_LOADER_H

#include <ReWizard/Disassembler/Disassembler.h>
#include <LIEF/Abstract.hpp>
#include <string>
#include <span>
#include <optional>
#include <memory>
#include <vector>
#include <unordered_map>

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

    using ArchPair = std::pair<ZydisMachineMode, ZydisStackWidth>;

    constexpr inline ArchPair InvalidArchPair{
        ZydisMachineMode::ZYDIS_MACHINE_MODE_MAX_VALUE,
        ZydisStackWidth::ZYDIS_STACK_WIDTH_MAX_VALUE
    };

    // Use the correct type for the map key, i.e., uint32_t for LIEF::Header::modes()
    inline std::unordered_map<uint32_t, ArchPair> ArchMap = {
        { LIEF::Header::BITS_64 , { ZydisMachineMode::ZYDIS_MACHINE_MODE_LONG_64,  ZydisStackWidth::ZYDIS_STACK_WIDTH_64  } },
        { LIEF::Header::BITS_32 , { ZydisMachineMode::ZYDIS_MACHINE_MODE_LEGACY_32, ZydisStackWidth::ZYDIS_STACK_WIDTH_32 } },
        { LIEF::Header::BITS_16 , { ZydisMachineMode::ZYDIS_MACHINE_MODE_REAL_16,   ZydisStackWidth::ZYDIS_STACK_WIDTH_16 } }
    };

    class FileLoader {
    public:
        static std::unique_ptr<FileLoader> Create(const std::string& name);
        ~FileLoader();

        bool Load(bool executable = false);

        static std::optional<size_t> GetSize(std::unique_ptr<LIEF::Binary>& t);
        static std::optional<size_t> GetHeaderSize(std::unique_ptr<LIEF::Binary>& t);

        FileLoaderStatus Status() const { return m_status; }
        std::span<uint8_t> Raw() { return m_mappedSpan; }
        const std::span<uint8_t> Raw() const { return m_mappedSpan; }
        size_t MappedSize() const { return m_mappedSize; }
        LIEF::Binary* Binary() { return m_target.get(); }
        ArchPair Arch() const { return m_arch; }
        uintptr_t CurrentImageBase() { return reinterpret_cast<uintptr_t>(m_mappedPtr); }

        bool IsWithinMapping(uint8_t* ptr) {
            return ptr >= m_mappedPtr && ptr < (m_mappedPtr + m_mappedSize);
        }

        bool IsWithinMapping(uintptr_t ptr) {
            auto p = reinterpret_cast<uint8_t*>(ptr);
            return p >= m_mappedPtr && p < (m_mappedPtr + m_mappedSize);
        }

    private:
        FileLoader(const std::string& name);
        bool LoadSections();

        void SetArch() {
            auto hdr = m_target->header();
            auto it = ArchMap.find(hdr.modes());
            m_arch = (it != ArchMap.end()) ? it->second : InvalidArchPair;
        }

        std::unique_ptr<LIEF::Binary>   m_target{ nullptr };
        std::vector<uint8_t>            m_raw{};
        std::span<uint8_t>              m_mappedSpan{};
        size_t                          m_mappedSize{ 0 };
        uint8_t*                        m_mappedPtr{ nullptr };
        FileLoaderStatus                m_status = FileLoaderStatus::Success;
        ArchPair                        m_arch{ InvalidArchPair };
    };

}

#endif
