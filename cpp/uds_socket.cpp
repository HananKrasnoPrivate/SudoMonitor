#include "uds_socket.h"

#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace SudoMonitor {
struct UdsSocket::Impl {
    std::string path;
    Mode _mode;
    int _commonFd = -1;
    std::vector<struct pollfd> _pollFds;
    OnNewData _onNewData;

    Impl(const std::string& p, Mode m, const OnNewData& onNewData) : path(p), _mode(m), _onNewData(onNewData)  {}

    void setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    ~Impl() {
        if (_commonFd != -1) close(_commonFd);
        for (auto& pfd : _pollFds) {
            if (pfd.fd != _commonFd) close(pfd.fd);
        }
        if (_mode == Mode::SERVER) unlink(path.c_str());
    }
};

UdsSocket::UdsSocket(const std::string& path, Mode _mode, const OnNewData& onNewData )
    : pimpl(std::make_unique<Impl>(path, _mode, onNewData)) {}

UdsSocket::~UdsSocket() = default;

bool UdsSocket::init() {

    pimpl->_commonFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (pimpl->_commonFd < 0) return false;

    pimpl->setNonBlocking(pimpl->_commonFd);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, pimpl->path.c_str(), sizeof(addr.sun_path) - 1);

    if (pimpl->_mode == Mode::SERVER) {
        unlink(pimpl->path.c_str());
        if (bind(pimpl->_commonFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return false;
        if (listen(pimpl->_commonFd, SOMAXCONN) < 0) return false;
        
        pimpl->_pollFds.push_back({pimpl->_commonFd, POLLIN, 0});
    } else {
        // Non-blocking connect usually returns -1 with EINPROGRESS
        connect(pimpl->_commonFd, (struct sockaddr*)&addr, sizeof(addr));
    }
    return true;
}

void UdsSocket::serverUpdate() {
    if (pimpl->_mode != Mode::SERVER)
        return;

    // Use a 0ms timeout for true non-blocking polling
    int ret = poll(pimpl->_pollFds.data(), pimpl->_pollFds.size(), 0);
    if (ret <= 0)
        return;
    for (size_t i = 0; i < pimpl->_pollFds.size(); ++i) {
        if (pimpl->_pollFds[i].revents & POLLIN) {
            if (pimpl->_pollFds[i].fd == pimpl->_commonFd) {
                // Accept new client
                int clientFd = accept(pimpl->_commonFd, nullptr, nullptr);
                if (clientFd >= 0) {
                    pimpl->setNonBlocking(clientFd);
                    pimpl->_pollFds.push_back({clientFd, POLLIN, 0});
                }
            } else {
                char buf[2048];
                ssize_t n = recv(pimpl->_pollFds[i].fd, buf, sizeof(buf) - 1, 0);
                if (n > 0) {
                    buf[n] = '\0';
                    if (pimpl->_onNewData)
                        pimpl->_onNewData(pimpl->_pollFds[i].fd, buf);
                } else if (n == 0) {
                    // Client closed connection
                    close(pimpl->_pollFds[i].fd);
                    pimpl->_pollFds.erase(pimpl->_pollFds.begin() + i);
                    --i;
                }
            }
        }
    }
}

bool UdsSocket::clientSend(const std::string& msg) {
    if (pimpl->_commonFd == -1)
        return false;
    ssize_t n = send(pimpl->_commonFd, msg.c_str(), msg.length(), MSG_DONTWAIT);
    return n > 0;
}
}