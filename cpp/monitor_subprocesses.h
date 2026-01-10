#pragma once

#include <functional>
#include <map>
#include <memory>

namespace SudoMonitor {
enum ProcStatEvent {Created, Died, Removed}; //TODO: add "Changed" event

struct ProcessData {
    using PropsMap = std::map<std::string, std::string>;
    pid_t pid = 0;
    pid_t ppid = 0;
    bool active = false;
    PropsMap props;
    ProcessData() = default;
    ProcessData(pid_t p, pid_t ppid) : pid(p), ppid(ppid), active(true) {}
};

class ProcTreeMonitor {
public:
    using OnProcStatChange = std::function<void(const ProcessData&, ProcStatEvent)>;
    explicit ProcTreeMonitor(const OnProcStatChange& cb = nullptr);
    ~ProcTreeMonitor();
    ProcTreeMonitor(const ProcTreeMonitor&) = delete;
    ProcTreeMonitor& operator=(const ProcTreeMonitor&) = delete;

    void addRootProc(pid_t pid);
    void rootProcDied(pid_t pid);
    void run();

private:
    struct Impl;           // Forward declaration of the implementation
    std::unique_ptr<Impl> pimpl;
};
}

std::ostream& operator<<(std::ostream& os, const SudoMonitor::ProcessData& pd);
std::ostream& operator<<(std::ostream& os, const SudoMonitor::ProcStatEvent& event);