#ifndef FILE_LOADER_H
#define FILE_LOADER_H

#include <LIEF/Abstract.hpp>
#include <string>
#include <span>

namespace ReWizard {
	class FileLoader {
	public:
		FileLoader(const std::string& name);

		bool Load(bool executable=false);

		static size_t GetSize(std::unique_ptr<LIEF::Binary> &t);
		static size_t GetHeaderSize(std::unique_ptr<LIEF::Binary>& t);

	private:
		bool LoadSections();

	private:
		std::unique_ptr<LIEF::Binary>	m_target{ nullptr };
		size_t							m_mappedSize{ 0 };
		uint8_t*						m_mappedPtr{ nullptr };
		std::span<uint8_t>				m_mappedSpan = {};
	};

	class FileLoaderProvider {

	};
}

#endif