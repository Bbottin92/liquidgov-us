#include "site/features/mail/mail_attachments.hpp"
#include "site/core/state.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"

namespace liquidgov::site {
// Suggested download name only (the browser lets the user rename); no
// original filename is stored, so this is derived from the mime that
// ingest_file actually assigned after content sniffing.
std::string attachment_filename_for_mime(const std::string& mime) {
    if (mime == "image/png") return "attachment.png";
    if (mime == "application/pdf") return "attachment.pdf";
    if (mime.rfind("text/plain", 0) == 0) return "attachment.txt";
    if (const ProgramKindInfo* k = program_kind_by_mime(mime)) return std::string("program") + k->extension;
    return "attachment";
}

// ---- Programs in mail (archives, installers, executables) ----
// Operators send every kind up to the server ceiling; anyone else only
// what an operator granted them on /admin/accounts (FileStore ProgramGrant).
// The ceiling is the upload protocol's maximum: the whole upload is held in
// memory while it's checked and stored, and this server has 0.5 GB.
std::uint64_t program_ceiling_bytes() { return g_mail_caps.protocol_max_bytes; }

std::optional<ProgramPolicy> program_policy_for(const std::string& username) {
    if (username.empty()) return std::nullopt;
    if (is_operator(username)) return ProgramPolicy{kAllProgramKinds, program_ceiling_bytes()};
    auto g = g_store.program_grant(username);
    if (!g || g->kinds == 0) return std::nullopt;
    return ProgramPolicy{g->kinds & kAllProgramKinds, std::min<std::uint64_t>(g->max_bytes, program_ceiling_bytes())};
}

std::string program_kinds_text(std::uint32_t kinds) {
    std::string out;
    for (const auto& k : program_kinds()) {
        if ((kinds & static_cast<std::uint32_t>(k.kind)) == 0) continue;
        if (!out.empty()) out += ", ";
        out += k.slug;
    }
    return out.empty() ? "none" : out;
}

std::string fmt_size(std::uint64_t bytes) {
    char b[32];
    if (bytes >= 1024ull * 1024) std::snprintf(b, sizeof b, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024) std::snprintf(b, sizeof b, "%.0f KB", static_cast<double>(bytes) / 1024.0);
    else std::snprintf(b, sizeof b, "%llu bytes", static_cast<unsigned long long>(bytes));
    return b;
}

// A program's download name: the sender's file name without any folder,
// only [A-Za-z0-9 ._()+-], at most 80 characters, ending in the extension
// of the kind the content actually is (a disguised .exe still saves as
// .exe). Safe to put in a Content-Disposition header as-is.
std::string sanitize_download_name(const std::string& raw, const ProgramKindInfo& kind) {
    std::string base = raw;
    auto slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    std::string name;
    for (char c : base) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == ' ' || c == '.' || c == '_' || c == '(' || c == ')' || c == '+' || c == '-';
        if (ok) name += c;
    }
    while (!name.empty() && (name.front() == '.' || name.front() == ' ')) name.erase(0, 1);
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    const std::string ext = kind.extension;
    auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    const bool has_ext = !ext.empty() && name.size() >= ext.size() &&
                         lower(name.substr(name.size() - ext.size())) == ext;
    if (has_ext) name.resize(name.size() - ext.size());
    if (name.size() > 80 - ext.size()) name.resize(80 - ext.size());
    if (name.empty()) name = "program";
    return name + ext;
}

// How an attachment shows in a mail bubble. A program gets a card with its
// name, size and who sent it, and a plain warning; anything else keeps the
// existing "Download attachment" link.
// `name_key` is the message's attachment_name_key; `sender_label` is how
// the bubble names the sender.
std::string mail_attachment_html(const std::string& blob_id, const std::string& mime, const std::string& name_key,
                                 const std::string& sender_label, bool mine) {
    const std::string href = "/mail/attachment/" + html_escape(blob_id);
    const ProgramKindInfo* kind = program_kind_by_mime(mime);
    if (!kind) return "<div><a href=\"" + href + "\">Download attachment</a></div>";
    const std::string name = g_store.attachment_name(name_key).value_or(attachment_filename_for_mime(mime));
    const auto meta = g_blobs.meta(blob_id);
    std::string html = "<div class=\"mailprog\"><a href=\"" + href + "\" download>"
                       "<span aria-hidden=\"true\">\xF0\x9F\x93\xA6</span> <strong>" + html_escape(name) + "</strong></a>"
                       "<span class=\"muted\"> " + html_escape(kind->label) +
                       (meta ? " \xC2\xB7 " + fmt_size(meta->size) : std::string()) + "</span>";
    if (!mine) html += "<p class=\"muted\">A program from " + html_escape(sender_label) +
                       ". Open it only if you trust them and expected it. Windows may warn that it comes from an "
                       "unknown publisher.</p>";
    return html + "</div>";
}

// Error text for a refused mail attachment, shared by 1:1 and group mail.
std::string mail_attachment_error_html(const std::string& e, const std::string& user) {
    if (e == "type") return "<p class=\"warn\">That attachment isn't a usable type. Allowed: PNG/JPEG/GIF, PDF, plain text, "
                            "macro-free Word/Excel/PowerPoint. No SVG.</p>";
    if (e == "large") {
        auto policy = program_policy_for(user);
        return "<p class=\"warn\">That attachment is too large (pictures 10&nbsp;MB, documents 25&nbsp;MB, video 50&nbsp;MB" +
               (policy ? ", programs " + fmt_size(policy->max_bytes) + " for you" : std::string()) + ").</p>";
    }
    if (e == "program") {
        auto policy = program_policy_for(user);
        if (!policy) return "<p class=\"warn\">That's a program (an archive, installer or app). Sending programs needs an "
                            "admin's permission; ask an admin if you need it.</p>";
        return "<p class=\"warn\">You can send these kinds of programs: " + html_escape(program_kinds_text(policy->kinds)) +
               ". That file is a different kind; ask an admin to add it.</p>";
    }
    if (e == "active") return "<p class=\"warn\">That document was refused: it declares a macro, embedded object, or OLE content.</p>";
    if (e == "scan") return "<p class=\"warn\">That attachment could not be stored.</p>";
    return {};
}

// The composer's file picker: what it offers, plus a line telling a
// program sender what they may send. `big_action` is the composer's
// /mail/big door: a program sender's form posts there only while a file
// over the ordinary limit is attached (second review 2026-10-27: posting
// every message there refused all mail from a small grant and put chat
// under the upload rate limit).
std::string mail_file_input_html(const std::string& user, const std::string& big_action) {
    std::string accept = "image/png,image/jpeg,image/gif,application/pdf,text/plain,.docx,.xlsx,.pptx";
    auto policy = program_policy_for(user);
    // The picked file's name and size show under the composer (.mailfile): the paperclip alone gave no sign a file
    // was attached (2026-10-27).
    std::string onchange = " onchange=\"var f=this.files&amp;&amp;this.files[0],F=this.form,s=F.querySelector('.mailfile');"
                           "if(s)s.textContent=f?'Attached: '+f.name+' ('+(f.size/1048576).toFixed(1)+' MB)':'';";
    if (policy) {
        for (const auto& k : program_kinds())
            if ((policy->kinds & static_cast<std::uint32_t>(k.kind)) && k.extension[0]) accept += std::string(",") + k.extension;
        if (!big_action.empty()) {
            const std::size_t ordinary = kMaxBody - kMultipartRoom; // what fits through plain /mail with its form fields
            onchange += "if(!F.dataset.small)F.dataset.small=F.getAttribute('action');"
                        "F.setAttribute('action',f&amp;&amp;f.size&gt;" + std::to_string(ordinary) +
                        "?'" + big_action + "':F.dataset.small);";
        }
    }
    return "<input type=\"file\" name=\"attachment\" accept=\"" + accept + "\"" + onchange + "\">";
}

std::string mail_program_note_html(const std::string& user) {
    auto policy = program_policy_for(user);
    if (!policy) return {};
    return "<p class=\"muted\" style=\"margin:.2rem 0 0\">You can send programs: " +
           html_escape(program_kinds_text(policy->kinds)) + ", up to " + fmt_size(policy->max_bytes) + ".</p>";
}
MailUpload take_mail_upload(const std::string& ctype, std::string& body) {
    MailUpload up;
    if (ctype.find("multipart/form-data") == std::string::npos) return up;
    up.filename = multipart_filename(ctype, body, "attachment");
    up.file = multipart_file(ctype, body, "attachment");
    std::string().swap(body);
    return up;
}

// Ingest a mail attachment with the sender's program policy. On success
// `download_name` is the sanitized name for a program ("" otherwise); the
// caller stores it with set_attachment_name once the message exists.
// Returns the ?err= code on refusal, "" on success.
std::string ingest_mail_attachment(const std::string& user, const MailUpload& up, std::optional<std::string>& blob_id,
                                   std::optional<std::string>& mime, std::string& download_name) {
    auto policy = program_policy_for(user);
    auto r = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *up.file, policy ? &*policy : nullptr);
    if (r.error != FileTransferError::None || !r.blob_id) {
        switch (r.error) {
            case FileTransferError::TooLarge: return "large";
            case FileTransferError::ActiveContentBlocked: return "active";
            case FileTransferError::FailedScan: return "scan";
            case FileTransferError::ProgramNotAllowed: return "program";
            default: return "type";
        }
    }
    blob_id = r.blob_id;
    mime = r.stored_mime_type;
    if (const ProgramKindInfo* kind = program_kind_by_mime(mime.value_or("")))
        download_name = sanitize_download_name(up.filename, *kind);
    return {};
}

// The download of a mail attachment the caller already authorized
// (`name_key`: that message's attachment_name_key). With
// LIQUIDGOV_BLOB_ACCEL set to the internal nginx location that maps to the
// blob folder ("/_lgblob/", deploy/nginx-program-mail.conf) every
// attachment goes out through nginx, streamed from disk. Without it only
// files up to 26 MB are sent from here (a 100 MB one would be held about
// four times on a 0.5 GB server); bigger ones get a 503 (review 2026-10-27).
std::string mail_attachment_response(const std::string& blob_id, const std::string& mime, const std::string& name_key) {
    const ProgramKindInfo* kind = program_kind_by_mime(mime);
    const std::string name = kind ? g_store.attachment_name(name_key).value_or(attachment_filename_for_mime(mime))
                                  : attachment_filename_for_mime(mime);
    const std::string extra = "Content-Disposition: attachment; filename=\"" + name + "\"\r\n"
                              "Cache-Control: private, no-store\r\n";
    const auto meta = g_blobs.meta(blob_id);
    if (!meta || blob_id.size() <= 2) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
    const char* accel = std::getenv("LIQUIDGOV_BLOB_ACCEL");
    if (accel && accel[0]) {
        return http_raw("200 OK", mime.c_str(), "",
                        extra + "X-Accel-Redirect: " + accel + blob_id.substr(0, 2) + "/" + blob_id + "\r\n");
    }
    if (meta->size > kMaxBody)
        return http_raw("503 Service Unavailable", "text/plain; charset=utf-8",
                        "This file is too big to send without the server's download handoff.");
    auto bytes = g_blobs.get(blob_id);
    if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
    return http_raw("200 OK", mime.c_str(), std::string(bytes->begin(), bytes->end()), extra);
}

// Big uploads have one door: POST /mail/big (1:1) and /mail/big/g/<id>/send
// (groups). nginx lets a body over 26 MB through only there, and only after
// GET /internal/upload-ok said yes for that cookie and Content-Length
// (deploy/nginx-program-mail.conf), so nobody else can make nginx or this
// process buffer 100 MB (review 2026-10-27).
bool is_big_upload_target(const std::string& target) {
    if (target == "/mail/big") return true;
    return target.rfind("/mail/big/g/", 0) == 0 && target.size() > 17 &&
           target.compare(target.size() - 5, 5, "/send") == 0;
}

// The body limit for a program sender on the big door: their limit plus
// room for the multipart wrapping; kMaxBody for everyone and everything
// else. Caller holds g_mu.
std::size_t program_body_limit_locked(const std::string& headers) {
    auto sess = g_store.get_session(cookie_of(headers));
    if (!sess) return kMaxBody;
    auto acc = g_store.find_by_id(sess->account);
    if (!acc) return kMaxBody;
    auto policy = program_policy_for(acc->username);
    if (!policy) return kMaxBody;
    return std::max<std::size_t>(kMaxBody, static_cast<std::size_t>(policy->max_bytes) + kMultipartRoom);
}

// The limit read_http_request enforces, from the headers alone (before the
// body is read). Doesn't touch the session (handle() does that).
std::size_t request_body_limit(const std::string& headers) {
    auto le = headers.find("\r\n");
    std::istringstream is(headers.substr(0, le));
    std::string method, target;
    is >> method >> target;
    target = target.substr(0, target.find('?'));
    if (method != "POST" || !is_big_upload_target(target)) return kMaxBody;
    std::lock_guard<std::mutex> lock(g_mu);
    return program_body_limit_locked(headers);
}

// nginx's auth_request for /mail/big: 204 when this cookie's account may
// send programs and X-Original-Length fits its limit (or the ordinary one,
// whichever is bigger), 403 otherwise (nginx then never reads the body).
// No length (a chunked upload) is a no: nginx couldn't hold it to a size.
// Caller holds g_mu.
std::string upload_ok_response(const std::string& headers) {
    std::string uri = header_value(headers, "X-Original-URI");
    uri = uri.substr(0, uri.find('?'));
    std::size_t length = 0;
    try { length = static_cast<std::size_t>(std::stoull(header_value(headers, "X-Original-Length"))); } catch (...) { length = 0; }
    bool granted = false;
    if (auto sess = g_store.get_session(cookie_of(headers)))
        if (auto acc = g_store.find_by_id(sess->account)) granted = program_policy_for(acc->username).has_value();
    const bool ok = granted && is_big_upload_target(uri) && length > 0 && length <= program_body_limit_locked(headers);
    return ok ? http_raw("204 No Content", "text/plain; charset=utf-8", "")
              : http_raw("403 Forbidden", "text/plain; charset=utf-8", "no");
}

}  // namespace liquidgov::site
