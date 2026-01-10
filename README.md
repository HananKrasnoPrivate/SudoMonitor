# 🛡️ SudoMonitorSystem (SudoMonitor)

A comprehensive C++17 framework for auditing and monitoring `sudo` sessions and PAM authentication attempts in real-time.

## 🚀 Overview
SudoAuditSystem intercepts privileged command executions and authentication events. It utilizes a custom sudo I/O plugin and a PAM module to provide deep visibility into what happens during an elevated session, including real-time process tree tracking via the `/proc` filesystem.



## 📂 Project Structure
* **`cpp/`**: Core implementation logic.
    * `sudo_plugin.cpp`: Sudo I/O plugin for intercepting command data.
    * `sudo_pam_module.cpp`: PAM module for tracking auth attempts (is not ready yet).
    * `sudo_monitor_daemon.cpp`: Centralized collection service.
    * `monitor_subprocesses.cpp`: Background monitoring of process lifecycles.
    * `uds_socket.cpp`: Inter-process communication via Unix Domain Sockets.
    * `simulator.cpp`: Test utility to simulate events without system-wide changes.
* **`go/`**: Supplementary tools and real-time UI dashboards (Is not ready yet).
* **`CMakeLists.txt`**: Build configuration.
* **`build.sh` / `test.sh`**: Automation scripts for building and validation.

## 🛠️ Building
The project requires a C++17 compatible compiler and CMake.
The easiest way to build the project is to run `./build.sh`. You can run `./build.sh -h` for more options.
It is also possible to build the project manually using CMake or from an IDE.

## ⚙️ Configuration
The project configuration is divided into two files:
* **`cpp/common.h`**: Local hardcoded configuration options that can be moved to a configuration JSON or INI file.
* **`/etc/sudo.conf`**: Dedicated sudo configuration file used to enable the sudo I/O plugin.

The default configuration doesn't require any optional parameters and can be set like this:
`Plugin sudo_plugin_conf /usr/local/lib/plugin.so`
In this case, the plugin will work silently without any logs and IPC.

## 📊 Audit Management Parameters
The following is the list of optional parameters that manages audit operations:

| Parameter | Description |
| :--- | :--- |
| `console_log=true` | Enables console logging. |
| `log_file=/tmp/sudo_plugin.log` | Sets the log file path and enables file logging. |
| `use_daemon=true` | Sends the start/end sudo sessions to the daemon. The daemon is responsible for listening and monitoring the sudo processes related data. It supports multiple simultaneous sudo simulation sessions from different sudo commands. |
| `monitor_tree=true` | Enables monitoring of the sudo process tree inside the sudo process. It exists for debug purposes. If `use_daemon=true` is set, it cancels the process tree monitoring inside the sudo. |

---

## 🧪 Running the Tests
The project comes with a simple test utility that can be used to simulate sudo events.
The simplest way for testing the project is to use the following configuration in the `/etc/sudo.conf` file:
`Plugin sudo_plugin_conf /usr/local/lib/plugin.so console_log=true log_file=/tmp/sudo_plugin.log use_daemon=true`

In parallel, run the `sudo_monitor_daemon` from the build directory. After that, each sudo process and subprocess will be monitored by the system and the statuses will be printed in the console, the log file, and by the daemon.

### Supported Process Lifecycle Events:

* **`started`**: The sudo process/subprocess has started.
* **`died`**: The sudo process/subprocess has ended.
* **`removed`**: All sudo processes/subprocesses have died and all orphaned subprocesses have been removed.

---

## 📅 Opened Issues (Planned for Sunday)
The following issues are estimated to be solved during a 3–4 hour window:
* **Netlink Integration**: The `sudo_monitor_daemon` currently uses the `/proc` tree polling approach only. For real-time monitoring, a Netlink socket connector has to be added to the process tree monitor.
* **PAM Module**: The PAM module is not ready yet. The communication with the daemon is not implemented and it was not tested.
* **Go UI**: The Go-based UI monitor currently doesn't communicate with the daemon.