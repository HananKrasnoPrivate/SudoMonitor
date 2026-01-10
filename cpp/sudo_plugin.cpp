#include "monitor_subprocesses.h"
#include "common.h"
#include "uds_socket.h"
#include "protocol.h"

#include <sudo_plugin.h>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <map>
#include <functional>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <filesystem>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

static sudo_printf_t plugin_printf = nullptr;
static sudo_printf_t log_printf = nullptr;
static FILE *log_file = nullptr;
static bool use_daemon = false;
static bool use_monitor = false;
static std::unique_ptr<SudoMonitor::UdsSocket> clientSocket;
static std::unique_ptr<SudoMonitor::ProcTreeMonitor> monitorTree;

void logToFile(const char *fmt, ...) {
    if (!log_file)
        return;
    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    fflush(log_file);

}
#define LOG(level, fmt, ...)  if (log_printf) log_printf(level, fmt "\n", ##__VA_ARGS__); logToFile(fmt, ##__VA_ARGS__)


#define LOG_INFO(fmt, ...) LOG(SUDO_CONV_INFO_MSG,  "[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(SUDO_CONV_ERROR_MSG, "[ERROR] " fmt "\n", ##__VA_ARGS__)

using ParameterHandler = std::function<void(const char*)>;
using Handlers = std::map<std::string, ParameterHandler>;

static Handlers handlers = {
    {"console_log", [](const char* value) {if (strcmp(value, "true") == 0) log_printf = plugin_printf;}},
    {"log_file", [](const char* value) {log_file = fopen(value, "w");}}, //TODO: error handling and pid/time as filename part
    {"use_daemon", [](const char* value) {use_daemon = strcmp(value, "true") == 0;}}, //TODO: socket path instead "true"
    {"monitor_tree", [](const char* value) {use_monitor = strcmp(value, "true") == 0;}}
};


void apply_custom_options(char* const options[])
{
    try {
        if (!options || !*options || handlers.empty())
            return;

        for (size_t i = 0; options[i] != nullptr; ++i) {
            const char* line = options[i];
            if (!*line)
                continue;

            // Find '='
            const char* value = std::strchr(line, '=');
            if (!value || value == line || !value[1]) {
                // no '=' or empty key → skip
                continue;
            }
            auto name = std::string(line, value - line);
            value++;
            auto handler = handlers.find(name);
            if (handler == handlers.end()) {
                // no handler for this name → ignore (or log)
                continue;
            }
            handler->second(value);
        }
    } catch (...) {
        //TODO: error handling
    }
}
static bool connect_to_socket(const std::string& path) {
    if (clientSocket)
        return true;
    clientSocket = std::make_unique<SudoMonitor::UdsSocket>(path, SudoMonitor::UdsSocket::Mode::CLIENT);
    return clientSocket->init();
}

static bool send_to_socket(const SudoMonitor::SudoMsg& msg) {
    if (!clientSocket)
        return false;
    return clientSocket->clientSend(msg.toString());
}

static void log_info(const char* prefix, char * const info[]) {
    if (info) {
        for (int i = 0; info[i] != NULL; i++) {
            LOG_INFO("%s info %i: %s", prefix, i, info[i]);
        }
    }
}


int (*open)(unsigned int version, sudo_conv_t conversation,
    sudo_printf_t sudo_printf, char * const settings[],
    char * const user_info[], char * const command_info[],
    int argc, char * const argv[], char * const user_env[],
    char * const plugin_options[]);
static int sudo_open(
    unsigned int version,
    sudo_conv_t conversation,    // Conversation function (for user prompts)
    sudo_printf_t printf_fn,      // Logging function
    char * const settings[],      // Plugin settings
    char * const user_info[],      // Info about the user (UID, TTY, etc.)
    char * const command_info[],  //command info
    int argc,
    char * const argv[],
    char * const user_env[],       // User's environment variables
    char * const options[]  // Options passed to plugin in sudo.conf
    )
{
    try {
        plugin_printf = printf_fn;
        apply_custom_options(options);
        LOG_INFO("--- Sudo Plugin Started: %i ---", getpid());
        log_info("User", user_info);
        log_info("Command", command_info);

        if (use_daemon) {
            connect_to_socket(SudoMonitor::Config::SudoToDaemonSock);
            send_to_socket({SudoMonitor::SudoMsgType::START_SESSION, getpid()});
        } else if (use_monitor) {
            monitorTree = std::make_unique<SudoMonitor::ProcTreeMonitor>([&](const SudoMonitor::ProcessData& data, SudoMonitor::ProcStatEvent stat){
               std::stringstream ss;
                ss << "Process: " << data.pid << "; Event: " << stat << "; Props: " << data;
                auto tmp = ss.str();
                LOG_INFO("=== %s", ss.str().c_str());
            });
            // monitorTree->run();
            monitorTree->addRootProc(getpid());
        }
        // Use sudo's internal logger
     } catch(...) {
        //TODO: error handling
    }
    return 1; // Success
}
static void sudo_close(int exit_status, int error) {
    if (monitorTree) {
        monitorTree->rootProcDied(getpid());
        monitorTree.reset();
    }
    send_to_socket({SudoMonitor::SudoMsgType::END_SESSION, getpid()});

    if (error != 0) {
        LOG_ERROR("Sudo command failed to execute. Error: %d", error);
    } else {
        LOG_INFO("Sudo session closed. Exit status: %d", exit_status);
    }
}

extern "C" {
    struct io_plugin sudo_plugin_conf = {
        SUDO_IO_PLUGIN,
        SUDO_API_VERSION,
        sudo_open,
        sudo_close, // close
        NULL, // show_version
        NULL, // log_ttyin
        NULL, // log_ttyout
        NULL, // log_stdin
        NULL, // log_stdout
        NULL, // log_stderr
        NULL, // register_hooks
        NULL  // unregister_hooks
    };
}