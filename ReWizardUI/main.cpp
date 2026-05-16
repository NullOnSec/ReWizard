#include <ReWizard/ReWizard.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>

// TODO: Dear ImGui integration
// #include <imgui.h>
// #include <imgui_impl_glfw.h>
// #include <imgui_impl_opengl3.h>

void PrintUsage(const char* program) {
    std::cerr << "ReWizard UI - Interactive Binary Analysis\n"
              << "Usage: " << program << " <target-binary> [options]\n"
              << "Options:\n"
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

    spdlog::info("ReWizardUI: starting interactive analysis for {}", target);

    // TODO: Initialize Dear ImGui window
    // TODO: Create analysis manager and run passes in background thread
    // TODO: Render interactive panels:
    //   - Disassembly view
    //   - Graph view (Boost.Graph-derived CFG)
    //   - Hex view
    //   - Function list
    //   - Cross-reference panel
    //   - Console/scripting

    auto manager = ReWizard::AnalysisManager::Create(target);
    if (!manager) {
        spdlog::error("Failed to create analysis manager for: {}", target);
        return 1;
    }

    manager->Run();
    spdlog::info("Analysis completed for target: {}", manager->Name());

    // TODO: Open analysis database in UI
    // TODO: Enter main render loop

    spdlog::info("ReWizardUI: shutting down");
    return 0;
}
