add_library (ReWizardLib STATIC
	"source/Disassembler/Disassembler.cpp"
	"source/FileLoader/FileLoader.cpp"
	"source/Analysis/Units/BasicBlock.cpp"
	"source/Analysis/Units/Function.cpp"
	"source/Analysis/Units/Module.cpp"
	"source/Analysis/Passes/StaticGenericPass.cpp"
	"source/Analysis/AnalysisContext.cpp"
	"source/Analysis/AnalysisManager.cpp"
	"source/Analysis/PassManager.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardLib PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardLib PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/ReWizard"
)

target_link_libraries(ReWizardLib PRIVATE LIEF::LIEF Zydis Boost::headers)