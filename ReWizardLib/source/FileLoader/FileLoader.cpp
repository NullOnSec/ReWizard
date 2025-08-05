#include <ReWizard/FileLoader/FileLoader.h>

#include <LIEF/PE.hpp>
#include <LIEF/ELF.hpp>
#include <LIEF/MachO.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ReWizard {

	FileLoader::FileLoader(const std::string& name) {
		m_target = LIEF::Parser::parse(name);
	}

	bool FileLoader::Load(bool exec) {
		m_mappedSize = GetSize(m_target);
		auto headerSize = GetHeaderSize(m_target);
		if (m_mappedSize == -1 || headerSize == -1)
			return false;

		auto base = m_target->imagebase();
		auto hdr = m_target->header();
		
		/* we are allocating more memory than we actually need  */
		m_mappedPtr = (uint8_t*)VirtualAlloc(reinterpret_cast<LPVOID>(base), m_mappedSize, MEM_COMMIT, exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
		if (!m_mappedPtr) {
			m_mappedPtr = (uint8_t*)VirtualAlloc(0, m_mappedSize, MEM_COMMIT, exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
			if (!m_mappedPtr) return false;
		}

		m_mappedSpan = std::span<uint8_t>(m_mappedPtr, m_mappedSize);

		auto raw = m_target->get_content_from_virtual_address(m_target->imagebase(), m_target->original_size());
		
		memcpy(m_mappedSpan.data(), raw.data(), raw.size());

	}

	bool FileLoader::LoadSections() {
		auto sections = m_target->sections();

		for (auto& section : sections) {
			auto data = m_target->get_content_from_virtual_address(section.virtual_address(), section.size());
			memcpy(m_mappedSpan.data(), data.data(), data.size());
		}

	}

	size_t FileLoader::GetHeaderSize(std::unique_ptr<LIEF::Binary>& t) {
		switch (t->format()) {
			case LIEF::Binary::ELF: {
				return t->as<LIEF::ELF::Binary>()->header().header_size();
			}
			case LIEF::Binary::MACHO: {
				auto macho = t->as<LIEF::MachO::Binary>();
				const auto& header = macho->header();
				// 0xFEEDFACF and 0xCFFAEDFE are 64-bit, 0xFEEDFACE/0xCEFAEDFE are 32-bit

				uint32_t magic = (uint32_t)header.magic();
				size_t hdr_size = (magic == 0xFEEDFACF || magic == 0xCFFAEDFE) ? 32 : 28;
				return hdr_size + header.sizeof_cmds();
			}
			case LIEF::Binary::PE: {
				return t->as<LIEF::PE::Binary>()->sizeof_headers();
			}
			default:
				break;
		}

		return -1;

	}

	size_t FileLoader::GetSize(std::unique_ptr<LIEF::Binary> &t) {
		switch (t->format()) {
			case LIEF::Binary::ELF: {
				return t->as<LIEF::ELF::Binary>()->virtual_size();
			}
			case LIEF::Binary::MACHO: {
				return t->as<LIEF::MachO::Binary>()->virtual_size();
			}
			case LIEF::Binary::PE: {
				return t->as<LIEF::PE::Binary>()->virtual_size();
			}
			default:
				break;
		}

		return -1;
	}

}
