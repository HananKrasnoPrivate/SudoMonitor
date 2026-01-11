#include "uds_socket.h"
#include "protocol.h"

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <syslog.h>

#include "common.h"

std::unique_ptr<SudoMonitor::UdsSocket> clientSocket;
std::string username;
void log(const std::string& msg) {
    static bool wasInitialized = false;
    if (!wasInitialized) {
        openlog("SudoMonitorPAM", LOG_PID | LOG_CONS, LOG_AUTH);
        wasInitialized = true;
    }
    syslog(LOG_INFO, "PAM_CUSTOM_INFO %s", msg.c_str());
}
void notify_daemon(const std::string& msg) {
    if (!clientSocket) {
        clientSocket = std::make_unique<SudoMonitor::UdsSocket>(SudoMonitor::Config::SudoToDaemonSock, SudoMonitor::UdsSocket::Mode::CLIENT);
        clientSocket->init(); //TODO: add error handling
    }
    auto nullTerminated = msg + "";
    clientSocket->clientSend(nullTerminated);
    log(std::string("Sent message: " + nullTerminated));
}

// Called by PAM during the authentication phase
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char *user = nullptr;
    pam_get_user(pamh, &user, NULL);
    username = user ? user : "unknown";
    std::string smsg = "pam_auth_attempt " + username;
    notify_daemon(smsg);

    // We return PAM_IGNORE because we aren't deciding IF the user can log in,
    // we are just "tapping" the line to listen.
    return PAM_IGNORE;
}

// Called after the real auth module (like pam_unix) finishes
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    std::string smsg = "pam_auth_success " + username;
    notify_daemon(smsg);
    return PAM_SUCCESS;
}