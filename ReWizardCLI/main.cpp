#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <ReWizard/ReWizard.h>

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " <target-binary> [options]\n"
              << "Options:\n"
              << "  --verbose    Enable verbose (debug) logging\n"
              << "  --help       Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string target(argv[1]);

    if (target == "--help" || target == "-h") {
        PrintUsage(argv[0]);
        return 0;
    }

    for (int i = 2; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--verbose" || arg == "-v") {
            spdlog::set_level(spdlog::level::debug);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    auto manager = ReWizard::AnalysisManager::Create(target);
    if (!manager) {
        spdlog::error("Failed to create analysis manager for: {}", target);
        return 1;
    }

    manager->Run();
    spdlog::info("Analysis completed for target: {}", manager->Name());

    return 0;
}
