#ifndef API_SET_MAPPINGS_HPP
#define API_SET_MAPPINGS_HPP

#include <ReWizard/Utils/Utils.h>

#include <Windows.h>
#include <winternl.h>
#include <string>
#include <vector>
#include <map>

namespace ReWizard {
#define API_SET_SCHEMA_ENTRY_FLAGS_SEALED 0

	typedef struct _API_SET_NAMESPACE {
		ULONG Version;
		ULONG Size;
		ULONG Flags;
		ULONG Count;
		ULONG EntryOffset;
		ULONG HashOffset;
		ULONG HashFactor;
	} API_SET_NAMESPACE, * PAPI_SET_NAMESPACE;

	typedef struct _API_SET_HASH_ENTRY {
		ULONG Hash;
		ULONG Index;
	} API_SET_HASH_ENTRY, * PAPI_SET_HASH_ENTRY;

	typedef struct _API_SET_NAMESPACE_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG HashedLength;
		ULONG ValueOffset;
		ULONG ValueCount;
	} API_SET_NAMESPACE_ENTRY, * PAPI_SET_NAMESPACE_ENTRY;

	typedef struct _API_SET_VALUE_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG ValueOffset;
		ULONG ValueLength;
	} API_SET_VALUE_ENTRY, * PAPI_SET_VALUE_ENTRY;

	using ApiSetMappings = std::map<std::string, std::vector<std::string>>;

	ApiSetMappings& DumpApiSetMappings(bool force = false) {
		static ApiSetMappings apiSetMappings;
		if (apiSetMappings.empty() || force) {
			apiSetMappings.clear();
			auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
			auto apiSetMap = static_cast<PAPI_SET_NAMESPACE>(peb->Reserved9[0]);
			auto apiSetMapAsNumber = reinterpret_cast<ULONG_PTR>(apiSetMap);
			auto nsEntry = reinterpret_cast<PAPI_SET_NAMESPACE_ENTRY>(apiSetMapAsNumber + apiSetMap->EntryOffset);

			for (ULONG i = 0; i < apiSetMap->Count; i++, nsEntry++) {
				auto nameW = reinterpret_cast<const wchar_t*>(apiSetMapAsNumber + nsEntry->NameOffset);
				auto nameLen = nsEntry->NameLength / sizeof(wchar_t);
				if (!nameLen) continue;
				std::wstring nameWStr(nameW, nameLen);
				std::string name = WideToCString(nameWStr);
				name += ".dll"; // Append .dll to the name
				auto valueEntry = reinterpret_cast<PAPI_SET_VALUE_ENTRY>(apiSetMapAsNumber + nsEntry->ValueOffset);
				std::vector<std::string> values;
				for (ULONG j = 0; j < nsEntry->ValueCount; j++, valueEntry++) {
					auto valW = reinterpret_cast<const wchar_t*>(apiSetMapAsNumber + valueEntry->ValueOffset);
					auto valLen = valueEntry->ValueLength / sizeof(wchar_t);
					std::wstring valWStr(valW, valLen);
					std::string valStr = WideToCString(valWStr);
					values.push_back(valStr);
				}
				apiSetMappings[name] = std::move(values);
			}
		}

		return apiSetMappings;
	}

}

#endif