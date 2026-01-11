#pragma once
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <iostream>

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
static inline void two_digits(char* p, int v) {
    p[0] = char('0' + (v/10));
    p[1] = char('0' + (v%10));
}

static inline std::string getLogTime() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);                  // ns precision
    std::tm tm;
    localtime_r(&ts.tv_sec, &tm);
    char out[16]; // 8 + 1 + 6 + 1 = 16
    two_digits(out+0,  tm.tm_hour);
    out[2] = ':';
    two_digits(out+3,  tm.tm_min);
    out[5] = ':';
    two_digits(out+6,  tm.tm_sec);
    out[8] = '.';

    int usec = int(ts.tv_nsec / 1000);
    out[9]  = char('0' + (usec/100000)%10);
    out[10] = char('0' + (usec/10000)%10);
    out[11] = char('0' + (usec/1000)%10);
    out[12] = char('0' + (usec/100)%10);
    out[13] = char('0' + (usec/10)%10);
    out[14] = char('0' + (usec)%10);
    out[15] = 0;

    return out;
}

static inline std::ostream& logPrefix(std::ostream& os) {
     return os << "[" << getLogTime() << "] ";
}
}