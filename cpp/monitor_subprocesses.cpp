#include "monitor_subprocesses.h"
#include "common.h"

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <dirent.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <sstream>
#include <csignal>
#include <unistd.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <sys/socket.h>


namespace SudoMonitor {
namespace { //namespace for local helpers
using PropsList = std::vector<std::string>;

std::string readProcFile(const std::string& pid, const std::string& fileName) {
    try {
        std::string path = "/proc/" + pid + "/" + fileName;
        std::ifstream stat_file(path);
        if (!stat_file.is_open())
            return "";
        return static_cast<std::stringstream&>(std::stringstream() << std::ifstream(path).rdbuf()).str();
    } catch (...) { //TODO: add error handling
        return "";
    }
}

std::string readProcFile(pid_t pid, const std::string& fileName) {
    return readProcFile(std::to_string(pid), fileName);
}
const std::string& statFieldNameByIndex(uint index, bool dynamicOnly) {
    struct Fld{const std::string name; bool dynamic;};
    static const std::vector<Fld> fields = {
        {"", false},             // 0: Placeholder (unused)
        {"pid", false},          // 1  pid     	    The process ID.
        {"comm", false},         // 2  comm        	The executable filename (in parentheses).
        {"state", true},         // 3  state       	Process state (R=Running, S=Sleeping, D=Disk sleep, Z=Zombie, T=Stopped).
        {"ppid", false},         // 4  ppid        	The Parent Process ID.
        {"pgrp", false},         // 5  pgrp        	The Process Group ID.
        {"session", false},      // 6  session     	The Session ID of the process.
        {"tty_nr", false},       // 7  tty_nr      	The controlling terminal of the process.
        {"tpgid", false},        // 8  tpgid       	The ID of the foreground process group of the controlling terminal.
        {"flags", false},        // 9  flags       	The kernel flags word of the process.
        {"minflt", true},        // 10 minflt      	Number of minor faults (didn't require loading a memory page from disk).
        {"cminflt", false},      // 11 cminflt     	Number of minor faults made by waited-for children.
        {"majflt", true},        // 12 majflt      	Number of major faults (required loading a memory page from disk).
        {"cmajflt", true},       // 13 cmajflt     	Number of major faults made by waited-for children.
        {"utime", false},        // 14 utime       	CPU time spent in user mode (in clock ticks).
        {"stime", true},         // 15 stime       	CPU time spent in kernel mode (in clock ticks).
        {"cutime", false},       // 16 cutime      	User CPU time of waited-for children (in clock ticks).
        {"cstime", false},       // 17 cstime      	Kernel CPU time of waited-for children (in clock ticks).
        {"priority", true},      // 18 priority    	Standard scheduling priority (+15 to +20).
        {"nice", false},         // 19 nice        	The nice value (quality of "neighborliness" to others).
        {"num_threads", true},   // 20 num_threads  Number of threads in this process.
        {"itrealvalue", false},  // 21 itrealvalue  Time (in jiffies) before next SIGALRM (obsolete).
        {"starttime", false},    // 22 starttime    Time the process started after system boot (in clock ticks).
        {"vsize", true},         // 23 vsize       	Virtual memory size in bytes.
        {"rss", true}            // 24 rss     	    Resident Set Size: number of pages the process has in real RAM.
    };
    if (index >= fields.size() || dynamicOnly && !fields[index].dynamic)
        return fields[0].name;
    return fields[index].name;
}
void createUpdateProcessData(ProcessData& processData, const PropsList& props) {
    bool isOld  = !processData.props.empty();
    uint index = 1;
    for (const auto& prop : props) {
        auto name = statFieldNameByIndex(index++, isOld);
        if (!name.empty())
            processData.props[name] = prop;
    }
    if (!isOld) {
        auto cmd = readProcFile(processData.pid, "cmdline");
        if (!cmd.empty())
            processData.props["cmdline"] = cmd;
    }
}
struct Node {
    using SubProc = std::vector<Node>;
    ProcessData processData;
    SubProc subProc;

    Node(pid_t p, pid_t ppid = 0) : processData(p, ppid) {}

    bool active() const { return processData.active; }
    pid_t pid() const { return processData.pid; }
    bool orphan() const { return processData.props.count("orphan") > 0; }
    void died() {
        processData.active = false;
        for (auto& sub : subProc)
            sub.processData.props["orphan"] = "true";
    }
    int findChild(pid_t pid) const {
        for (int i = 0; i < subProc.size(); ++i) {
            if (subProc[i].processData.pid == pid) return i;
        }
        return -1;
    }
};

PropsList conditionalSplit(const std::string& s, pid_t ppid) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, ' ')) {
        if (ppid > 0 && tokens.size() == 3 && token != std::to_string(ppid))
            return {};
        tokens.push_back(token);
    }
    return tokens;
}
std::string ProcStatEventToString(ProcStatEvent event) {
    switch (event) {
        case ProcStatEvent::Created: return "Created";
        case ProcStatEvent::Died: return "Died";
        case ProcStatEvent::Removed: return "Removed";
        default: return "Unknown";
    }
}

bool isPidAlive(pid_t pid) {
    if (pid <= 0)
        return false;
    if (::kill(pid, 0) == 0)
        return true;
    if (errno == EPERM)
        return true;
    return false;
}
using ProcList = std::vector<std::pair<pid_t, PropsList>>;
PropsList statToProcList(const std::string& pid, pid_t ppid = 0) {
    auto content = readProcFile(std::stoi(pid), "stat");
    return conditionalSplit(content, ppid);
}
ProcList getChildrenFromOS(pid_t ppid) {
    ProcList children;
    DIR* dir = opendir("/proc");
    if (!dir) return children;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (isdigit(entry->d_name[0])) {
            pid_t pid = atoi(entry->d_name);
            auto props = statToProcList(std::to_string(pid), ppid);
            if (props.empty())
                continue;
            children.emplace_back(pid, std::move(props));
        }
    }
    closedir(dir);
    return children;
}

}

struct ProcTreeMonitor::Impl {
    const std::chrono::milliseconds TreeUpdateTimeout{5};
    std::map<pid_t, Node> _processTrees;
    OnProcStatChange _onProcStatChange;
    std::thread _treeUpdateWorker;
    std::thread _netLinkWorker;
    std::mutex _mtx;
    std::atomic<bool> _running{false};
    std::condition_variable _cv;
    bool _eventTriggered = false;

    explicit Impl(const OnProcStatChange& cb) : _onProcStatChange(cb) {}
    ~Impl() {
        _running = false;
        if (_treeUpdateWorker.joinable())
            _treeUpdateWorker.join();
        if (_netLinkWorker.joinable())
            _netLinkWorker.join();
    }
    void syncActiveState(Node& node) {
        if (!node.active())
            return;
        if (isPidAlive(node.pid()))
            return;
        node.died();
        if (_onProcStatChange)
            _onProcStatChange(node.processData, ProcStatEvent::Died);
    }
    void syncProcessData(Node& node) {
        const auto props = statToProcList(std::to_string(node.pid()));
        createUpdateProcessData(node.processData, statToProcList(std::to_string(node.pid())));
    }
    void triggerUpdateTree() {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _eventTriggered = true;
        }
        _cv.notify_one();
    }
    void syncNode(Node& node) {
        syncActiveState(node);
        if (node.active() && node.orphan()) {
            syncProcessData(node);
        }
        const auto osChildren = getChildrenFromOS(node.pid());
        for (const auto& childProps : osChildren) {
            auto childIndex = node.findChild(childProps.first);
            if (childIndex < 0) {
                Node newNode(childProps.first, node.processData.pid);
                createUpdateProcessData(newNode.processData, childProps.second);
                if (_onProcStatChange)
                    _onProcStatChange(newNode.processData, ProcStatEvent::Created);
                node.subProc.emplace_back(newNode);
            } else {
                createUpdateProcessData(node.subProc[childIndex].processData, childProps.second);
            }
        }

        for (auto it = node.subProc.begin(); it != node.subProc.end(); ) {
            syncNode(*it);
            if (!it->processData.active && it->subProc.empty()) {
                if (_onProcStatChange)
                    _onProcStatChange(it->processData, ProcStatEvent::Removed);
                it = node.subProc.erase(it);
            } else {
                ++it;
            }
        }
    }
    int nl_open() {
        int s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
        if (s < 0) { perror("socket"); return -1; }
        sockaddr_nl sa = { .nl_family = AF_NETLINK, .nl_pid = static_cast<uint>(getpid()) , .nl_groups = CN_IDX_PROC};
        if (bind(s, (sockaddr*)&sa, sizeof(sa)) < 0) { perror("bind"); close(s); return -1; }
        // subscribe
        struct Req {
            struct nlmsghdr nlh;
            struct cn_msg   cn;
            enum proc_cn_mcast_op op;
        };
        Req req;
        req.nlh.nlmsg_len = sizeof(req);
        req.nlh.nlmsg_pid = static_cast<uint32_t>(getpid());
        req.nlh.nlmsg_type = NLMSG_DONE;

        req.cn.id.idx = CN_IDX_PROC;
        req.cn.id.val = CN_VAL_PROC;
        req.cn.len = sizeof(enum proc_cn_mcast_op);

        req.op = PROC_CN_MCAST_LISTEN;
        if (send(s, &req, sizeof(req), 0) < 0) { perror("send"); close(s); return -1; }
        return s;
    }

    void runNetLinkLoop() {
        try {
            int s = nl_open();
            if (s < 0)
                return;
            unsigned char buf[4096];
            while (_running) {
                ssize_t n = recv(s, buf, sizeof(buf), 0);
                if (n <= 0) {
                    if (errno == EINTR)
                        continue;
                    perror("recv");
                    break;
                }

                nlmsghdr *nlh = (struct nlmsghdr*)buf;
                for (; NLMSG_OK(nlh, n); nlh = NLMSG_NEXT(nlh, n)) {
                    cn_msg *cn = (struct cn_msg*)NLMSG_DATA(nlh);
                    proc_event *ev = (struct proc_event*)cn->data;

                    switch (ev->what) {
                        case proc_event::PROC_EVENT_FORK: {
                            // pid_t ppid = ev->event_data.fork.parent_pid;
                            // pid_t cpid = ev->event_data.fork.child_pid;
                            // std::cout << "Netlink: Fork event detected, parent: " << ppid << ", child: " << cpid << std::endl;
                        }
                            triggerUpdateTree();
                            break;
                        case proc_event::PROC_EVENT_EXEC: {
                            auto cmd = readProcFile(ev->event_data.exec.process_tgid, "cmdline");
                            // logPrefix(std::cout) << "Netlink: exec event detected, pid: " << ev->event_data.exec.process_tgid << ", cmd: " << cmd << std::endl;

                            break;
                        }
                        case proc_event::PROC_EVENT_EXIT:
                            // logPrefix(std::cout) << "Netlink: exit event detected, pid: " << ev->event_data.exit.process_tgid << std::endl;
                            break;
                        default: break;
                    }
                }
            }
            close(s);
        } catch (const std::exception& e) {
            std::cerr << "Netlink error: " << e.what() << std::endl;
        }
    }
    void run() {
        _running = true;
        _netLinkWorker = std::thread([this] { runNetLinkLoop(); });
        _treeUpdateWorker = std::thread([this]() {
            while (_running) {
                std::unique_lock<std::mutex> lock(_mtx);
                _cv.wait_for(lock, TreeUpdateTimeout, [this] {
                    return !_running || _eventTriggered;
                });
                    for (auto it = _processTrees.begin(); it != _processTrees.end();) {
                        const auto& [pid, node] = *it;
                        syncNode(it->second);
                        if (!node.active() && node.subProc.empty()) {
                            if (_onProcStatChange)
                                _onProcStatChange(node.processData, ProcStatEvent::Removed);
                            it = _processTrees.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
        });
    }
};
// Public API Bridge
ProcTreeMonitor::ProcTreeMonitor(const OnProcStatChange& cb)
    : pimpl(std::make_unique<Impl>(cb)) {}

ProcTreeMonitor::~ProcTreeMonitor() = default;

void ProcTreeMonitor::addRootProc(pid_t pid) {
    {
        std::lock_guard<std::mutex> lock(pimpl->_mtx);
        if (pimpl->_processTrees.find(pid) == pimpl->_processTrees.end()) {
            Node root(pid);
            pimpl->syncProcessData(root);
            if (pimpl->_onProcStatChange)
                pimpl->_onProcStatChange(root.processData, Created);
            pimpl->_processTrees.emplace(pid, root);

        } else {
            //TODO: error handling
            return;
        }
    }
    pimpl->triggerUpdateTree();
}

void ProcTreeMonitor::rootProcDied(pid_t pid) {
    std::lock_guard<std::mutex> lock(pimpl->_mtx);
    auto it = pimpl->_processTrees.find(pid);
    if (it != pimpl->_processTrees.end()) {
        it->second.died();
        if (it->second.subProc.empty()) {
            if (pimpl->_onProcStatChange)
                pimpl->_onProcStatChange(it->second.processData, Removed);
            pimpl->_processTrees.erase(it);
        }
    }
}

void ProcTreeMonitor::run() {
    pimpl->run();
}
}

std::ostream& operator<<(std::ostream& os, const SudoMonitor::ProcessData& pd) {
    for (auto it = pd.props.begin(); it != pd.props.end(); ++it) {
        os << it->first << ": " << it->second.c_str() << "; ";
    }
    return os;
}
std::ostream& operator<<(std::ostream& os, const SudoMonitor::ProcStatEvent& event) {
    return os << SudoMonitor::ProcStatEventToString(event);
}