add_executable (ReWizard "ReWizard.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizard PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(ReWizard PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)


