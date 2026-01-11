#include "common.h"
#include "uds_socket.h"

#include <iostream>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <cstdarg>
#include <sudo_plugin.h>


// Typical PAM function signature
typedef int (*pam_func)(void*, int, int, const char**);

void* loadSO(const std::string& path) {
    if (access(path.c_str(), R_OK) != 0) {
        throw std::runtime_error(("Module path provided " + path +  " does not exist or is not readable"));
    }
    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        throw std::runtime_error(" Error loading module " + path + ": " + dlerror());
    }
    std::cout << "-> Loaded " << path << std::endl;
    return handle;
}

void simulatePAM() {
    auto pam_handle = loadSO("./pam_custom_module.so");
    auto pam_auth = (pam_func)dlsym(pam_handle, "pam_sm_authenticate");
    if (pam_auth) {
        std::cout << "-> Simulating PAM Authentication Attempt..." << std::endl;
        pam_auth(nullptr, 0, 0, nullptr); // Triggers notify_daemon
    } else {
        std::cerr << "The method pam_handle is not found" << std::endl;
    }
    dlclose(pam_handle);
}

// This is your mock implementation of the sudo_printf_t callback
int mock_sudo_printf(int msg_type, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int printed = vprintf(fmt, args);
    va_end(args);
    return printed;
}

void simulateSudo() {
    void *sudo_handle = loadSO("./plugin.so");
    auto plugin = (io_plugin*) dlsym(sudo_handle, "sudo_plugin_conf");
    if (!plugin) {
        std::cerr << "-> Plugin io is not loaded" << std::endl;
        return;
    }
    std::cout << "-> Simulating Sudo Open Event..." << std::endl;
    char *user_info[] = {(char *) "user=simulated_user", nullptr};
    char *settings[] = {nullptr};
    char *options[] = {(char*) "console_log=true", (char*) "log_file=/tmp/sudo_plugin.log", (char*) "use_daemon=true", nullptr};
    char *user_env[] = {nullptr};
    char *command_info[] = {(char *) "command=/usr/bin/simulator", nullptr};

    plugin->open(SUDO_API_VERSION,
           nullptr, // Conversation function (for user prompts)
           mock_sudo_printf, // Logging function
           settings, // Plugin settings
           user_info, // Info about the user (UID, TTY, etc.)
           command_info, // Info about the command (Path, args)
           0,
           nullptr,
           user_env, // User's environment variables
           options   // Options passed to plugin in sudo.conf
           );

    std::cout << "-> Command Running (Sleeping 2s)..." << std::endl;
    SLEEP_MS(2000);

    std::cout << "-> Simulating Sudo Close Event..." << std::endl;
    plugin->close(0, 0); // Exit status 0
    dlclose(sudo_handle);
}


void simulateSendMsg() {
    SudoMonitor::UdsSocket client(SudoMonitor::Config::SudoToDaemonSock, SudoMonitor::UdsSocket::Mode::CLIENT);
    client.init();
    client.clientSend("Hello from the client!");
    SLEEP_MS(1000);
    // std::cout << "Response: " << client.receiveResponse() << std::endl;
}
int main() {
    std::cout << "Starting Sudo/PAM Plugin Simulator..." << std::endl;

    // simulatePAM();
    simulateSudo();
    // simulateSendMsg();
    std::cout << "Simulation complete." << std::endl;

    return 0;
}