cmake_minimum_required(VERSION 3.10)
project(SudoAuditSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_library(PAM_LIB pam REQUIRED)
find_package(Threads REQUIRED)

find_path(PAM_INCLUDE_DIR NAMES security/pam_modules.h REQUIRED)

# 3. Sudo Plugin (plugin.so)
add_library(sudo_plugin SHARED
        cpp/sudo_plugin.cpp
        cpp/monitor_subprocesses.cpp
        cpp/uds_socket.cpp

        cpp/monitor_subprocesses.h
        cpp/uds_socket.h
)
set_target_properties(sudo_plugin PROPERTIES 
    OUTPUT_NAME "plugin"
    PREFIX ""
)

target_include_directories(sudo_plugin PRIVATE include)
target_link_libraries(sudo_plugin PRIVATE Threads::Threads)

add_library(pam_custom_module SHARED
        cpp/sudo_pam_module.cpp
        cpp/uds_socket.cpp
)
set_target_properties(pam_custom_module PROPERTIES
    OUTPUT_NAME "pam_custom_module"
    PREFIX ""
)
target_include_directories(pam_custom_module PRIVATE ${PAM_INCLUDE_DIR})
target_link_libraries(pam_custom_module PRIVATE ${PAM_LIB} )

add_executable(sudo_daemon
        cpp/sudo_monitor_daemon.cpp
        cpp/monitor_subprocesses.cpp
        cpp/uds_socket.cpp

        cpp/monitor_subprocesses.h
        cpp/uds_socket.h
)
target_link_libraries(sudo_daemon PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)

# 5. Go UI Monitor (ui_monitor)
# We use a custom command to build the Go binary so 'make' triggers it.
find_program(GO_EXECUTABLE go)
if(GO_EXECUTABLE)
    add_custom_target(ui_monitor ALL
        COMMAND ${GO_EXECUTABLE} build -o ${CMAKE_BINARY_DIR}/ui_monitor ${CMAKE_CURRENT_SOURCE_DIR}/go/ui_monitor.go
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/go
        COMMENT "Building Go UI Monitor..."
    )
else()
    message(WARNING "Go compiler not found. ui_monitor will not be built.")
endif()

add_executable(simulator
        cpp/simulator.cpp
        cpp/uds_socket.cpp
)
target_link_libraries(simulator PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)