#include <iostream>

#include <ReWizard/ReWizard.h>

int main() {
	auto loader = ReWizard::FileLoader::Create(R"(C:\Users\z\Downloads\Launcher\target.exe)");
	loader->Load();
	return 0;
}
