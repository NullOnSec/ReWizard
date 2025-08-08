add_library (ReWizardLib STATIC
	"source/Disassembler/Disassembler.cpp"
	"source/Analysis/Manager.cpp"
	"source/FileLoader/FileLoader.cpp"
 "include/ReWizard/Analysis/Units/BasicBlock.h" "include/ReWizard/Analysis/Units/Function.h" "include/ReWizard/Analysis/Units/Module.h" "source/Analysis/Units/BasicBlock.cpp" "source/Analysis/Units/Function.cpp" "source/Analysis/Units/Module.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardLib PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardLib PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/ReWizard"
)

target_link_libraries(ReWizardLib PRIVATE LIEF::LIEF Zydis Boost::headers)