#include <iostream>

#include <ReWizard/ReWizard.h>

#include <string_view>

constexpr inline std::string_view target(R"(C:\Users\z\Downloads\Launcher\target.exe)");

int main() {
    auto context = ReWizard::AnalysisContext::Create(target.data());
    auto manager = ReWizard::AnalysisManager::Create(context);

    manager->Run();


	return 0;
}
