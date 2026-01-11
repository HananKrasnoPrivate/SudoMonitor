#include <atomic>
#include <csignal>

#include "common.h"
#include "monitor_subprocesses.h"
#include "uds_socket.h"
#include "protocol.h"

#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>


namespace SudoMonitor {
int in_fd, out_fd;
std::vector<int> clients;
class Daemon {
public:
    Daemon() :
    _server(Config::SudoToDaemonSock, UdsSocket::Mode::SERVER,
        [this](int fd, const std::string& data)->void {
        onNewData(fd, data);
    }),
    _procTreeMonitor([this](const ProcessData& data, ProcStatEvent stat)->void {

        logPrefix(std::cout) << "Process: " << data.pid << "; Event: " << stat << "; Props: " << data << std::endl;
    })
    {}
    ~Daemon() {
        _running = false;
        for (int fd : clients)
            close(fd);
        close(in_fd); close(out_fd);
        unlink(Config::SudoToDaemonSock);
        // unlink(Config::DaemonToMonitorSock);
    }
    void onNewData(int fd, const std::string& data) {
        auto msg = parseSudoMsg(data);
        switch (msg.type) {
            case SudoMsgType::START_SESSION:
                _procTreeMonitor.addRootProc(msg.pid());
                break;
            case SudoMsgType::END_SESSION:
                _procTreeMonitor.rootProcDied(msg.pid());
                break;
            default:  //TODO: add actions for PAM messages
                logPrefix(std::cout) << "Got message: " << data << std::endl;
                break;
        }

    }
    void runDaemon() {
        _server.init();
        _procTreeMonitor.run();
        _running = true;
        while(_running) {
            _server.serverUpdate();
        }
    }
private:
    std::atomic_bool _running = false;
    UdsSocket _server;
    ProcTreeMonitor _procTreeMonitor;
};
}
using namespace SudoMonitor;
void cleanup(int sig) {
    std::cout << "Exiting..." << std::endl;
    exit(0);
}
int main() {
    signal(SIGINT, cleanup);
    Daemon daemon;
    daemon.runDaemon();
    return 0;
}
