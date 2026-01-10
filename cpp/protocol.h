#pragma once
#include <string>

namespace SudoMonitor {
static constexpr auto SUDO_UNKNOWN = "UNKNOWN";
static constexpr auto SUDO_START_SESSION_CMD = "sudo_session_start";
static constexpr auto SUDO_END_SESSION_CMD = "sudo_session_end";
enum class SudoMsgType {
    UNKNOWN = 0,
    START_SESSION,
    END_SESSION,
    NUM_OF_MSG_TYPES
};

static std::string Messages[] = {SUDO_UNKNOWN, SUDO_START_SESSION_CMD, SUDO_END_SESSION_CMD};
struct SudoMsg {
    SudoMsgType type = SudoMsgType::UNKNOWN;
    pid_t pid = 0;
    std::string toString() const { return Messages[static_cast<uint>(type)] + " " + std::to_string(pid); }
};
inline std::string startSudoSession(pid_t  pid) {
    return SUDO_START_SESSION_CMD + std::to_string(pid);
}

inline SudoMsg parseSudoMsg(const std::string& msg) {
    int ind = 1;
    for (int i = 1; i < static_cast<int>(SudoMsgType::NUM_OF_MSG_TYPES); i++) {
        std::string m = Messages[i];
        if (msg.size() > m.size() && msg.substr(0, m.size()) == m) {
            std::string pid_str = msg.substr(m.size());
            try {
                return {SudoMsgType(ind), std::stoi(pid_str)}; //TODO: make better method for str-to-type conversion
            } catch (...) {
                return {};
            }
        }
        ind++;
    }
    return {};
}
}
