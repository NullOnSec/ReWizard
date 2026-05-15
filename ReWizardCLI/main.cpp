#include <iostream>
#include <string>
#include <fstream>

#include <spdlog/spdlog.h>
#include <ReWizard/ReWizard.h>
#include <ReWizard/Analysis/AnalysisResult.h>

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " <target-binary> [options]\n"
              << "Options:\n"
              << "  --verbose    Enable verbose (debug) logging\n"
              << "  --output     Output file path (default: stdout)\n"
              << "  --format     Output format: json, dot, text (default: text)\n"
              << "  --trace      Path to hybrid analysis trace file (JSON)\n"
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

    std::string outputPath;
    std::string format = "text";
    std::string tracePath;

    for (int i = 2; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--verbose" || arg == "-v") {
            spdlog::set_level(spdlog::level::debug);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                outputPath = argv[++i];
            } else {
                std::cerr << "Error: --output requires a path argument\n";
                return 1;
            }
        } else if (arg == "--format" || arg == "-f") {
            if (i + 1 < argc) {
                format = argv[++i];
            } else {
                std::cerr << "Error: --format requires a format argument (json, dot, text)\n";
                return 1;
            }
        } else if (arg == "--trace" || arg == "-t") {
            if (i + 1 < argc) {
                tracePath = argv[++i];
            } else {
                std::cerr << "Error: --trace requires a path argument\n";
                return 1;
            }
        }
    }

    auto manager = ReWizard::AnalysisManager::Create(target);
    if (!manager) {
        spdlog::error("Failed to create analysis manager for: {}", target);
        return 1;
    }

    if (!tracePath.empty()) {
        manager->Context()->SetTracePath(tracePath);
    }

    manager->Run();
    spdlog::info("Analysis completed for target: {}", manager->Name());

    auto result = ReWizard::AnalysisResult::FromContext(manager->Context());

    std::string output;
    if (format == "json") {
        output = result.ToJson();
    } else if (format == "dot") {
        output = result.ToDot();
    } else if (format == "text") {
        output = result.ToText();
    } else {
        std::cerr << "Error: unknown format '" << format << "'. Use json, dot, or text.\n";
        return 1;
    }

    if (!outputPath.empty()) {
        std::ofstream ofs(outputPath);
        if (!ofs) {
            std::cerr << "Error: cannot write to " << outputPath << "\n";
            return 1;
        }
        ofs << output;
    } else {
        std::cout << output;
    }

    return 0;
}
