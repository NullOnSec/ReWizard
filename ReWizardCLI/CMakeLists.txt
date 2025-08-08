add_executable (ReWizardCli "main.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardCli PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizardCli PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/ReWizardLib/include"
)

target_link_libraries(ReWizardCli PRIVATE LIEF::LIEF Zydis Boost::headers ReWizardLib)