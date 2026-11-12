// One shared "GET this HTTPS URL via the system curl binary" helper --
// see /jukebox/search's own comment for why curl-via-popen rather than a
// linked TLS/HTTP stack (this binary doesn't link libcurl, and curl is
// already on the box). Factored out of what used to be two near-
// identical inline popen() blocks; a third call site (Google tokeninfo
// verification) is why it's a real function now instead of a third
// copy. Caller must url_encode anything untrusted before it lands in
// url -- this does not escape anything itself.
// curl_exit (optional) gets curl's exit status: 0 when an answer came back (an HTTP error page too: no -f), curl's
// own code when nothing did (6 no DNS, 7 refused, 28 timeout, 35 TLS...), -1 when curl could not be started. Site
// sweep 2026-11-09: it was thrown away, so "unreachable" looked like an empty answer. curl's error line itself
// already reaches the service log (-sS prints it to our stderr).
std::string curl_get(const std::string& url, int timeout_sec, int* curl_exit = nullptr) {
    std::string cmd = "curl -sS --max-time " + std::to_string(timeout_sec) + " '" + url + "'";
    std::string resp;
    int status = -1;
    if (FILE* p = popen(cmd.c_str(), "r")) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) resp.append(buf, n);
        int st = pclose(p);
        status = (st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
    }
    if (curl_exit) *curl_exit = status;
    return resp;
}

// Best-effort password-reset delivery, tried in this order:
//   1. RESEND_API_KEY set -> Resend's HTTPS API (POST, JSON, via curl --
//      the one call site in this file that needs curl to POST, not GET).
//   2. LIQUIDGOV_SMTP_URL set -> a raw SMTP send via curl's own smtp://
//      support, message piped over the child's stdin (popen in "w" mode)
//      rather than ever appearing in the command line. UNVERIFIED beyond
//      code review -- there's no SMTP server or real credentials
//      anywhere in this dev environment to test it against. Test path 3
//      below first; only trust this once it's been tried against a real
//      mailbox.
//   3. Neither set -> log the reset URL to stderr (captured by journald
//      under systemd: `journalctl -u liquidgov`) instead of mailing
//      anything. This is the actually-testable path and is what "so
//      Brandon can test" in the spec refers to.
// Never throws, never returns anything the caller could use to change
// the HTTP response -- POST /forgot shows the same page regardless of
// whether this silently failed, on purpose (see that route's comment on
// not becoming an account-enumeration oracle via mail-delivery status).
void send_password_reset_email(const std::string& to_email, const std::string& reset_url) {
    const char* resend_key = std::getenv("RESEND_API_KEY");
    const char* smtp_url = std::getenv("LIQUIDGOV_SMTP_URL");
    const std::string subject = "Reset your LiquidGov.US password";
    const std::string body =
        "Someone (hopefully you) asked to reset the password on your LiquidGov.US account.\n\n"
        "This link works once, for the next hour:\n" + reset_url + "\n\n"
        "If you didn't ask for this, you can ignore it -- your password hasn't changed.";

    if (resend_key && resend_key[0]) {
        std::string json = "{\"from\":\"LiquidGov.US <noreply@liquidgov.us>\",\"to\":[\"" +
            json_escape(to_email) + "\"],\"subject\":\"" + json_escape(subject) +
            "\",\"text\":\"" + json_escape(body) + "\"}";
        std::string cmd = "curl -sS --max-time 8 -X POST https://api.resend.com/emails "
            "-H " + shell_single_quote("Authorization: Bearer " + std::string(resend_key)) + " "
            "-H 'Content-Type: application/json' "
            "-d " + shell_single_quote(json) + " -o /dev/null -w '%{http_code}'";
        std::string code;
        if (FILE* p = popen(cmd.c_str(), "r")) {
            char buf[16]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), p)) > 0) code.append(buf, n);
            pclose(p);
        }
        std::cerr << "[password-reset] resend send to " << (code == "200" || code == "202" ? "ok" : "FAILED (http " + code + ")")
                   << std::endl;
        return;
    }

    if (smtp_url && smtp_url[0]) {
        const std::string from = "noreply@liquidgov.us";
        std::string message = "From: LiquidGov.US <" + from + ">\r\n"
            "To: <" + to_email + ">\r\n"
            "Subject: " + subject + "\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n\r\n" + body + "\r\n";
        std::string cmd = "curl -sS --max-time 10 " + shell_single_quote(smtp_url) +
            " --mail-from " + shell_single_quote(from) +
            " --mail-rcpt " + shell_single_quote(to_email) +
            " --upload-file - -o /dev/null";
        int status = -1;
        if (FILE* p = popen(cmd.c_str(), "w")) {
            fwrite(message.data(), 1, message.size(), p);
            status = pclose(p);
        }
        std::cerr << "[password-reset] smtp send " << (status == 0 ? "ok" : "FAILED (status " + std::to_string(status) + ")")
                   << std::endl;
        return;
    }

    // No mailer configured: the URL goes to stderr only, never to the
    // browser (that's how POST /forgot's own page copy already reads --
    // this is the deliberate "Brandon can still test locally" fallback).
    std::cerr << "[password-reset] no mailer configured -- reset URL for "
               << to_email << ": " << reset_url << std::endl;
}

