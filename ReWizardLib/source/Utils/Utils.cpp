#include <ReWizard/Utils/Utils.h>

#include <windows.h>

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace ReWizard {
	template<typename T>
	std::vector<uint8_t> PackLE(T value) {
		std::vector<uint8_t> out(sizeof(T));
		for (size_t i = 0; i < sizeof(T); ++i)
			out[i] = static_cast<uint8_t>(value >> (i * 8));
		return out;
	}

	template std::vector<uint8_t> PackLE<uint32_t>(uint32_t);
	template std::vector<uint8_t> PackLE<uint64_t>(uint64_t);
	template std::vector<uint8_t> PackLE<int32_t>(int32_t);
	template std::vector<uint8_t> PackLE<int64_t>(int64_t);

	template<typename T>
	T UnpackLE(const std::vector<uint8_t>& data) {
		T value = 0;
		size_t len = (std::min)(data.size(), sizeof(T));
		for (size_t i = 0; i < len; ++i)
			value |= static_cast<T>(data[i]) << (i * 8);
		return value;
	}

	template uint32_t UnpackLE<uint32_t>(const std::vector<uint8_t>&);
	template uint64_t UnpackLE<uint64_t>(const std::vector<uint8_t>&);
	template int32_t  UnpackLE<int32_t>(const std::vector<uint8_t>&);
	template int64_t  UnpackLE<int64_t>(const std::vector<uint8_t>&);

	std::wstring CStringToWide(const std::string& str) {
		if (str.empty()) return std::wstring();
		int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
		if (len == 0) return std::wstring();
		std::wstring wstr(len - 1, L'\0');
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], len);
		return wstr;
	}

	std::string WideToCString(const std::wstring& wstr) {
		if (wstr.empty()) return std::string();
		int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
		if (len == 0) return std::string();
		std::string str(len - 1, '\0');
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], len, NULL, NULL);
		return str;
	}

	bool InitializeSymbols(const std::string& cacheDir) {
		std::string symPath = "srv*" + cacheDir + "*https://msdl.microsoft.com/download/symbols";
		SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_DEBUG); // Enable debug output
		return SymInitialize(GetCurrentProcess(), symPath.c_str(), TRUE) != FALSE;
	}

	std::string AddrToSymbol(uintptr_t addr) {
		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
		PSYMBOL_INFO pSym = reinterpret_cast<PSYMBOL_INFO>(buffer);
		pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSym->MaxNameLen = MAX_SYM_NAME;

		DWORD64 disp = 0;
		if (SymFromAddr(GetCurrentProcess(), addr, &disp, pSym)) {
			return std::string(pSym->Name);
		}
		return {};
	}
	bool LoadModuleSymbols(const std::string& modulePath, uintptr_t base, uint64_t size) {
		DWORD64 result = SymLoadModuleEx(
			GetCurrentProcess(),
			nullptr,
			modulePath.c_str(),
			nullptr,
			base,
			size,
			nullptr,
			0
		);
		return result != 0;
	}

}
