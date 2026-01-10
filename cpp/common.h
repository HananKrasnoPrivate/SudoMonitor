#pragma once
#include <chrono>
#include <thread>
#include <map>

#define SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
//TODO: implement as a config file
namespace SudoMonitor {
    struct Config {
        static constexpr auto SudoToDaemonSock = "/tmp/sudo_audit.sock";
        static constexpr auto SudoToDaemonSockMode = 0666;
        static constexpr auto SudoToDaemonSockTimeoutMs = 10;
        static constexpr auto SocketBufSize = 4096;
        static constexpr auto DaemonToMonitorSock = "/tmp/ui_monitor.sock";
        static constexpr auto DaemonToMonitorSockMode = 0666; //to allow access for non-sudo user at the testing stage

    };

}