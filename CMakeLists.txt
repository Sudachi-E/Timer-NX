cmake_minimum_required(VERSION 3.10)
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
set(PLATFORM_SWITCH ON)
set(USE_DEKO3D ON)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)
project(SwitchTimer)
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/resources)
add_subdirectory(${BOREALIS_LIBRARY} ${CMAKE_BINARY_DIR}/borealis-lib)
set(SWITCH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library/lib/platforms/switch/switch_wrapper.c)
set(DEMO_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/include)
set(DEMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/activity/main_activity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/tab/components_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/tab/recycling_list_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/tab/settings_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/tab/text_test_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/tab/transform_tab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/view/captioned_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/demo/src/view/pokemon_view.cpp
)
set(APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/source/main.cpp ${SWITCH_SOURCES} ${DEMO_SOURCES})
program_target(${PROJECT_NAME} "${APP_SOURCES}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
target_link_libraries(${PROJECT_NAME} PRIVATE borealis)
target_include_directories(${PROJECT_NAME} PRIVATE ${DEMO_INCLUDE})
if (PLATFORM_SWITCH)
    gen_dksh("${CMAKE_BINARY_DIR}/resources/shaders")
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_NAME}" "1.0.0" ${PROJECT_NAME}.nacp
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
endif()
