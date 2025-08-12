#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <cstdint>
#include <string>

namespace ReWizard {
	template<typename T>
	std::vector<uint8_t> PackLE(T value);

	template<typename T>
	T UnpackLE(const std::vector<uint8_t>& data);


	std::wstring CStringToWide(const std::string& str);
	std::string WideToCString(const std::wstring& wstr);


	bool InitializeSymbols(const std::string& cacheDir = "C:\\symbols");
	std::string AddrToSymbol(uintptr_t addr);
	bool LoadModuleSymbols(const std::string& modulePath, uintptr_t base, DWORD64 size);

}

#endif
