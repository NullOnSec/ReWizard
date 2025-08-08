#include <iostream>

#include <ReWizard/ReWizard.h>

#include <string_view>

constexpr inline std::string_view target(R"(C:\Users\z\Downloads\Launcher\target.exe)");

int main() {
    auto context = ReWizard::AnalysisContext::Create(target.data());

    auto &disasm = ReWizard::Disassembler::Get(ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    // Disassemble an instruction
    uint8_t code[] = { 0x48, 0x89, 0xC0 }; // mov rax, rax
    auto instruction = disasm->DisassembleSingle<ReWizard::ExtendedInstruction>(code, sizeof(code));

    if (instruction) {
        std::string formatted = disasm->InstructionToString(instruction.get(), reinterpret_cast<uintptr_t>(code));
        std::cout << formatted << std::endl;

    }
	return 0;
}
