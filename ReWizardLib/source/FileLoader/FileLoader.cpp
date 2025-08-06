#include <ReWizard/FileLoader/FileLoader.h>

#include <fstream>
#include <stdexcept>

#include <LIEF/PE.hpp>
#include <LIEF/ELF.hpp>
#include <LIEF/MachO.hpp>
#include <LIEF/BinaryStream/BinaryStream.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ReWizard {

    std::unique_ptr<FileLoader> FileLoader::Create(const std::string& name) {
        auto instance = std::unique_ptr<FileLoader>(new FileLoader(name));

        if (instance->Status() != FileLoaderStatus::Success)
            return nullptr;

        return instance;
    }

    FileLoader::FileLoader(const std::string& name) {
        std::ifstream file(name, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            m_status = FileLoaderStatus::FileOpenError;
            return;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size <= 0) {
            m_status = FileLoaderStatus::EmptyFile;
            return;
        }

        m_raw.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(m_raw.data()), size);

        m_target = LIEF::Parser::parse(m_raw);
        if (!m_target) {
            m_status = FileLoaderStatus::LiefParserError;
            return;
        }
    }

    FileLoader::~FileLoader() {
        if (m_mappedPtr) {
            VirtualFree(m_mappedPtr, 0, MEM_RELEASE);
            m_mappedPtr = nullptr;
        }
    }

    bool FileLoader::Load(bool exec) {
        auto size_opt = GetSize(m_target);
        auto header_size_opt = GetHeaderSize(m_target);

        if (!size_opt || !header_size_opt) {
            m_status = FileLoaderStatus::InvalidSize;
            return false;
        }

        m_mappedSize = *size_opt;
        size_t headerSize = *header_size_opt;

        // Validate that we're not trying to allocate absurd amounts of memory
        if (m_mappedSize == 0 || m_mappedSize > (1ULL << 32)) {  // 4GB limit
            m_status = FileLoaderStatus::InvalidSize;
            return false;
        }

        auto base = m_target->imagebase();
        auto hdr = m_target->header();

        // Allocate memory for the entire binary
        m_mappedPtr = (uint8_t*)VirtualAlloc(reinterpret_cast<LPVOID>(base), m_mappedSize, MEM_COMMIT, exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
        if (!m_mappedPtr) {
            // If allocation at preferred base fails, try to allocate at any address
            m_mappedPtr = (uint8_t*)VirtualAlloc(0, m_mappedSize, MEM_COMMIT, exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
            if (!m_mappedPtr) {
                m_status = FileLoaderStatus::AllocationError;
                return false;
            }
        }

        ZeroMemory(m_mappedPtr, m_mappedSize);

        m_mappedSpan = std::span<uint8_t>(m_mappedPtr, m_mappedSize);

        // Copy the raw binary data to mapped memory
        if (m_raw.size() <= 0) {
            m_status = FileLoaderStatus::InvalidSize;
            return false;
        }

        // Map headers
        memcpy(m_mappedSpan.data(), m_raw.data(), header_size_opt.value());

        if (!LoadSections()) {
            VirtualFree(m_mappedPtr, 0, MEM_RELEASE);
            m_mappedPtr = nullptr;
            m_status = FileLoaderStatus::SectionLoaderError;
            return false;
        }

        return true;
    }

    bool FileLoader::LoadSections() {
        if (!m_target) return false;

        auto sections = m_target->sections();

        for (auto& section : sections) {
            // Validate section data exists
            auto data = m_target->get_content_from_virtual_address(section.virtual_address(), section.size());
            if (data.empty())
                continue;  // Skip invalid/empty sections

            // ensure we don't write past our allocated memory
            size_t offset = section.virtual_address();
            if (offset + data.size() > m_mappedSize) {
                m_status = FileLoaderStatus::OutOfBoundsError;
                return false;
            }

            memcpy(m_mappedSpan.data() + offset, data.data(), data.size());
        }

        return true;
    }

    std::optional<size_t> FileLoader::GetHeaderSize(std::unique_ptr<LIEF::Binary>& t) {
        if (!t) return std::nullopt;

        if (auto elf = dynamic_cast<LIEF::ELF::Binary*>(t.get())) {
            return elf->header().header_size();
        }

        if (auto macho = dynamic_cast<LIEF::MachO::Binary*>(t.get())) {
            const auto& header = macho->header();
            uint32_t magic = (uint32_t)header.magic();
            size_t hdr_size = (magic == 0xFEEDFACF || magic == 0xCFFAEDFE) ? 32 : 28;
            return hdr_size + header.sizeof_cmds();
        }

        if (auto pe = dynamic_cast<LIEF::PE::Binary*>(t.get())) {
            return pe->sizeof_headers();
        }

        return std::nullopt;  // Unsupported binary type
    }

    std::optional<size_t> FileLoader::GetSize(std::unique_ptr<LIEF::Binary>& t) {
        if (!t) return std::nullopt;

        if (auto elf = dynamic_cast<LIEF::ELF::Binary*>(t.get())) {
            return elf->virtual_size();
        }

        if (auto macho = dynamic_cast<LIEF::MachO::Binary*>(t.get())) {
            return macho->virtual_size();
        }

        if (auto pe = dynamic_cast<LIEF::PE::Binary*>(t.get())) {
            return pe->virtual_size();
        }

        return std::nullopt;  // Unsupported binary type
    }
}
