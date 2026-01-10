#pragma once

#include <functional>
#include <string>
#include <memory>

namespace SudoMonitor {

class UdsSocket {
public:
    enum class Mode { SERVER, CLIENT };
    using OnNewData = std::function<void(int fd, const std::string&)>;
    UdsSocket(const std::string& path, Mode mode, const OnNewData& onNewData = nullptr);
    ~UdsSocket();

    UdsSocket(const UdsSocket&) = delete;
    UdsSocket& operator=(const UdsSocket&) = delete;

    bool init();

    // For Server: Non-blocking check for incoming data
    void serverUpdate();

    // For Client: Non-blocking send
    bool clientSend(const std::string& msg);

private:
    struct Impl; // Forward declaration of the implementation
    std::unique_ptr<Impl> pimpl;
};
}