cmake_minimum_required(VERSION 3.15)

project("ReWizardUI")

add_executable(ReWizardUI
    "main.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ReWizardUI PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(ReWizardUI PRIVATE ReWizardLib BochsRuntime)
if (MSVC)
    target_link_options(ReWizardUI PRIVATE /FORCE:MULTIPLE)
endif()

# TODO: Add Dear ImGui, GLFW, and OpenGL/GL3W dependencies
# target_link_libraries(ReWizardUI PRIVATE imgui glfw opengl32)
