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
    PAM_AUTH_ATTEMPT,
    PAM_AUTH_SUCCESS,
    PAM_AUTH_START_SESSION,
    PAM_AUTH_END_SESSION,
    NUM_OF_MSG_TYPES
};

static std::string Messages[] = {"UNKNOWN",
    "sudo_session_start",
    "sudo_session_end",
    "pam_auth_attempt",
    "pam_auth_success",
    "pam_auth_start_session",
    "pam_auth_end_session"
};
struct SudoMsg {
    SudoMsgType type = SudoMsgType::UNKNOWN;
    std::string value;
    SudoMsg() = default;
    SudoMsg(SudoMsgType t, pid_t pid) : type(t), value(std::to_string(pid)) {}
    SudoMsg(SudoMsgType t, const std::string& v) : type(t), value(v) {}
    [[nodiscard]] pid_t pid() const { return static_cast<pid_t>(std::stoi(value)); }
    [[nodiscard]] std::string toString() const { return Messages[static_cast<uint>(type)] + " " + value; }
};
inline std::string startSudoSession(pid_t  pid) {
    return SUDO_START_SESSION_CMD + std::to_string(pid);
}

inline SudoMsg parseSudoMsg(const std::string& msg) {
    int ind = 1;
    for (int i = 1; i < static_cast<int>(SudoMsgType::NUM_OF_MSG_TYPES); i++) {
        std::string m = Messages[i];
        if (msg.size() > m.size() && msg.substr(0, m.size()) == m) {
            auto value = msg.substr(m.size());
            try {
                return {static_cast<SudoMsgType>(ind), value};
            } catch (...) {
                return {};
            }
        }
        ind++;
    }
    return {};
}
}
