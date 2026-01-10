#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <string>

// Helper to send data to your existing daemon
void notify_daemon(const std::string& msg) {
    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) return;

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/var/run/sudo_audit.sock", sizeof(addr.sun_path) - 1);

    sendto(sock, msg.c_str(), msg.length(), MSG_DONTWAIT,
           (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
}

// Called by PAM during the authentication phase
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char *user = nullptr;
    pam_get_user(pamh, &user, NULL);
    std::string username = user ? user : "unknown";

    notify_daemon("PAM_EVENT: AUTH_ATTEMPT | USER: " + username);

    // We return PAM_IGNORE because we aren't deciding IF the user can log in,
    // we are just "tapping" the line to listen.
    return PAM_IGNORE;
}

// Called after the real auth module (like pam_unix) finishes
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    return PAM_SUCCESS;
}