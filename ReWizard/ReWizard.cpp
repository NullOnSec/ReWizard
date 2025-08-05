#include <iostream>

#include <LIEF/Abstract.hpp>


int main() {
	auto obj = LIEF::Parser::parse(R"(C:\Users\z\Downloads\Launcher\target.exe)");
	return 0;
}
