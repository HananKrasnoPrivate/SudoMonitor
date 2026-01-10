# 🛡️ SudoMonitorSystem (SudoMonitor)

A comprehensive C++17 framework for auditing and monitoring `sudo` sessions and PAM authentication attempts in real-time.

## 🚀 Overview
SudoAuditSystem intercepts privileged command executions and authentication events. It utilizes a custom sudo I/O plugin and a PAM module to provide deep visibility into what happens during an elevated session, including real-time process tree tracking via the `/proc` filesystem.

## 📂 Project Structure
*   **`cpp/`**: Core implementation logic.
    *   `sudo_plugin.cpp`: Sudo I/O plugin for intercepting command data.
    *   `sudo_pam_module.cpp`: PAM module for tracking auth attempts.
    *   `sudo_monitor_daemon.cpp`: Centralized collection service.
    *   `monitor_subprocesses.cpp`: Background monitoring of process lifecycles.
    *   `uds_socket.cpp`: Inter-process communication via Unix Domain Sockets.
    *   `simulator.cpp`: Test utility to simulate events without system-wide changes.
*   **`go/`**: Supplementary tools and real-time UI dashboards.
*   **`CMakeLists.txt`**: Build configuration.
*   **`build.sh` / `test.sh`**: Automation scripts for building and validation.

## 🛠️ Building
The project requires a C++17 compatible compiler and CMake.
