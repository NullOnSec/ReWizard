#include <iostream>

#include <spdlog/spdlog.h>
#include <ReWizard/ReWizard.h>

#include <string_view>

//constexpr inline std::string_view target(R"(C:\Users\z\Downloads\Launcher\target.exe)");
constexpr inline std::string_view target(R"(C:\Users\NOYFB\Downloads\exlcus\Launcher_enc\Launcher\target.exe)");

int main() {
    auto manager = ReWizard::AnalysisManager::Create(target.data());

    manager->Run();
	spdlog::info("Analysis completed for target: {}", manager->Name());

	return 0;
}
