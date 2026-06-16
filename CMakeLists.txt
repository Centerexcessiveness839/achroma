cmake_minimum_required(VERSION 3.16)
project(Achroma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_RELEASE        "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

option(ENABLE_ASAN  "Enable AddressSanitizer"  OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer"   OFF)

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
elseif(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
    set(CMAKE_BUILD_TYPE Debug)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc.a
                    OUTPUT_VARIABLE GCC_LIB OUTPUT_STRIP_TRAILING_WHITESPACE)
    get_filename_component(GCC_LIB_DIR "${GCC_LIB}" DIRECTORY)
    if(EXISTS "${GCC_LIB_DIR}")
        add_link_options(-L"${GCC_LIB_DIR}" -Wl,-rpath-link,"${GCC_LIB_DIR}")
    endif()

    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
    check_cxx_source_compiles("int main(){}" ASAN_WORKS)
    if(NOT ASAN_WORKS AND ENABLE_ASAN)
        message(FATAL_ERROR
            "AddressSanitizer library not found.\n"
            "Install the runtime:  dnf install libasan\n"
            "Or skip sanitizers:   cmake -DENABLE_ASAN=OFF ...")
    endif()

    message(STATUS "Sanitizer enabled: forcing Debug build type")
    message(STATUS "  ASAN: ${ENABLE_ASAN}, UBSAN: ${ENABLE_UBSAN}, TSAN: ${ENABLE_TSAN}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets Concurrent)
find_package(qtermwidget6 REQUIRED)

set(SOURCES
    main.cpp
    src/utils.cpp
    src/terminal.cpp
    src/browser.cpp
    src/commands.cpp
    src/triggers.cpp
    src/ipc.cpp
    src/adblockinterceptor.cpp
    src/fuzzyfinder.cpp
    src/permissionbar.cpp
    src/splash.cpp
    src/window.cpp
)

qt6_add_resources(ACHROMA_RESOURCES resources.qrc)

add_executable(Achroma ${SOURCES} ${ACHROMA_RESOURCES})

target_link_libraries(Achroma PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Concurrent
    qtermwidget6
)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(achroma_test
        test/test.cpp
        test/test_helpers.h
        src/utils.cpp
    )
    target_include_directories(achroma_test PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(achroma_test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME AchromaTests COMMAND achroma_test)

    add_executable(test_adblock
        test/test_adblock.cpp
        src/adblockinterceptor.cpp
    )
    target_include_directories(test_adblock PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_adblock PRIVATE Qt6::Core Qt6::Test Qt6::WebEngineCore)
    add_test(NAME AdBlockTests COMMAND test_adblock)
    set_tests_properties(AdBlockTests PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

    add_executable(test_commands
        test/test_commands.cpp
        test/test_commands_helpers.h
        src/commands.cpp
        src/browser.cpp
        src/permissionbar.cpp
        src/terminal.cpp
        src/utils.cpp
        src/triggers.cpp
        src/ipc.cpp
        src/adblockinterceptor.cpp
    )
    target_include_directories(test_commands PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_commands PRIVATE
        Qt6::Core Qt6::Test Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets qtermwidget6)
    add_test(NAME CommandTests COMMAND test_commands)
    set_tests_properties(CommandTests PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

    add_executable(test_scripts
        test/test_scripts.cpp
        test/test_scripts_helpers.h
        src/utils.cpp
    )
    target_include_directories(test_scripts PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_scripts PRIVATE
        Qt6::Core Qt6::Test Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME ScriptTests COMMAND test_scripts)
    set_tests_properties(ScriptTests PROPERTIES ENVIRONMENT "QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu --no-sandbox")
    set_property(TEST ScriptTests APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:new_delete_type_mismatch=0")
endif()

include(GNUInstallDirs)
install(FILES $<TARGET_FILE:Achroma> DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        RENAME achroma)
install(FILES Achroma.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES achroma.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(PROGRAMS achroma-cli DESTINATION ${CMAKE_INSTALL_BINDIR})

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    @ONLY
)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(LINT_SOURCES ${SOURCES} test/test_commands_helpers.h test/test_helpers.h test/test_scripts_helpers.h)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE}
            --use-color
            -p ${CMAKE_BINARY_DIR}
            ${LINT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on source files..."
    )
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found — install it: dnf install clang-tools-extra"
    )
endif()
