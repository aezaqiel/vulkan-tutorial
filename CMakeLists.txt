cmake_minimum_required(VERSION 3.29)

project(vkrt C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "" FORCE)

option(GLFW_BUILD_DOCS OFF)
option(GLFW_INSTALL OFF)
add_subdirectory(vendor/glfw)

option(CGLM_SHARED "Shared build" OFF)
option(CGLM_STATIC "Static build" ON)
add_subdirectory(vendor/cglm)

set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(vendor/volk)

add_executable(${PROJECT_NAME} src/main.c)

target_include_directories(${PROJECT_NAME}
PRIVATE
    src
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    glfw
    cglm
    volk
)

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    GLFW_INCLUDE_NONE
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        GLFW_EXPOSE_NATIVE_WIN32
        VK_USE_PLATFORM_WIN32_KHR
    )
endif()

find_package(Vulkan REQUIRED COMPONENTS glslangValidator)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.vert"
    "shaders/*.frag"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND Vulkan::glslangValidator -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(${PROJECT_NAME} shaders)
