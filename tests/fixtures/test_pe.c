#include <windows.h>

__declspec(dllexport) int TestExport(int x) {
    return x * 2;
}

int main() {
    HMODULE h = GetModuleHandleA("kernel32.dll");
    FARPROC p = GetProcAddress(h, "VirtualAlloc");
    MessageBoxA(NULL, "Test", "Test", MB_OK);
    return p ? 0 : 1;
}
