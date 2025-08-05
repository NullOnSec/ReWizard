add_library (ReWizardLib STATIC "source/Disassembler/Disassembler.cpp" "include/ReWizard/Analysis/Manager.h" "source/Analysis/Manager.cpp" "include/ReWizard/FileLoader/FileLoader.h" "source/FileLoader/FileLoader.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardLib PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardLib PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/ReWizard"
)

target_link_libraries(ReWizardLib PRIVATE LIEF::LIEF Zydis)