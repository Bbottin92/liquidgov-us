#include "site/features/canvas/canvas_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"
#include "site/features/canvas/canvas.hpp"
#include "site/features/feed/composer.hpp"

namespace liquidgov::site {

std::optional<std::string> canvas_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/me/canvas") {
        if(!acc) return need_login();
        return pg("Canvas", canvas_tool_page(user, flash_error(qs), false));
    }
    if(method=="GET"&&path=="/me/portrait") {
        if(!acc) return need_login();
        return pg("Self-portrait", canvas_tool_page(user, flash_error(qs), true));
    }
    if(method=="GET"&&path=="/feed/canvas") {
        if(!acc) return need_login();
        return pg("Compose a post", canvas_tool_page(user, "", false, "/feed"));
    }

    if(method=="GET"&&path.rfind("/portrait/",0)==0) {
        const std::string who = path.substr(10);
        if (!valid_username(who)) return http_code("404 Not Found", "not found");
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto av = g_store.get(found->id);
        if (!av) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto bytes = g_blobs.get(av->blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"portrait.png\"\r\n");
    }

    // /canvas-frame/<username>/<index> -- one saved drawing snapshot.
    // Requested by the profile page's own background-image CSS
    // (server-generated, not user CSS -- see profile_canvas_css), and
    // by /me/canvas itself to reload a saved drawing back onto the
    // editing layer -- public either way, same as /portrait/.
    if(method=="GET"&&path.rfind("/canvas-frame/",0)==0) {
        auto rest = path.substr(14);
        auto slash = rest.find('/');
        std::string body404 = "not found";
        if (slash == std::string::npos) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        const std::string who = rest.substr(0, slash);
        int idx = -1;
        try { idx = std::stoi(rest.substr(slash + 1)); } catch (...) { idx = -1; }
        if (!valid_username(who) || idx < 0) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto frames = g_store.canvas_frames(found->id);
        if (static_cast<std::size_t>(idx) >= frames.size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto bytes = g_blobs.get(frames[static_cast<std::size_t>(idx)]);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"frame.png\"\r\n");
    }
    if(method=="POST"&&path=="/me/canvas/clear") {
        if(!acc) return need_login();
        g_store.clear_canvas_frames(acc->id);
        return http_redir("/me/canvas");
    }
    if(method=="POST"&&path=="/me/canvas/frame") {
        if(!acc) return need_login();
        // Reuses save_avatar's exact normalize/scan/size-cap pipeline (the
        // security-relevant part) via a throwaway AvatarStore that just
        // captures the resulting blob id instead of writing the account's
        // real single avatar slot -- canvas frames are an ordered list,
        // not a single value, so FileStore's own AvatarStore side can't be
        // reused directly for this.
        struct CaptureBlobId : AvatarStore {
            std::optional<Avatar> get(AccountId) const override { return std::nullopt; }
            void set(const Avatar& a) override { blob_id = a.blob_id; }
            void erase(AccountId) override {}
            std::string blob_id;
        } capture;
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "frame");
        if (!file || file->empty()) return http_redir("/me/canvas?err=frameimage");
        if (g_store.canvas_frames(acc->id).size() >= kCanvasMaxFrames)
            return http_redir("/me/canvas?err=framefull");
        auto r = save_avatar(g_blobs, capture, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me/canvas?err=framelarge");
        if (r.error != SaveAvatarError::None) return http_redir("/me/canvas?err=frameimage");
        g_store.add_canvas_frame(acc->id, capture.blob_id);
        return http_ok("ok");
    }
    if(method=="POST"&&path=="/me/canvas/mode") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        g_store.set_canvas_timelapse(acc->id, f["timelapse"]=="1");
        return http_redir("/me/canvas?ok=1");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
