#include "site/features/canvas/canvas.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/text.hpp"
#include "site/ui/assets.hpp"

namespace liquidgov::site {
// Shared drawing-tool engine: freehand brushes (pen, marker, spray, eraser,
// smudge), shape tools (line, rectangle, circle, with an optional filled
// variant), a paint-bucket flood fill, a rectangular select tool (copy/cut/
// paste, plus crop and resize against the selection), a text tool, up to
// three independently-undoable layers, image/video/gif import, and paste
// of an external image straight from the OS clipboard. Powers three pages
// that share every tool but differ in what "Save" does:
//   portrait=false, post_target empty -- /me/canvas, the profile-background
//     tool. Captures a PNG after every completed action (capped, see
//     kCanvasMaxFrames) by flattening the layers and uploading through
//     /me/canvas/frame -- the exact same normalize/scan pipeline as an
//     avatar upload, just appending to an ordered list instead of
//     replacing a single slot. Save turns that list into the profile
//     page's own identity-card background: last frame as a static image,
//     or all of them as a CSS-only looping animation (see
//     profile_canvas_css) -- no JS-driven replay, just background-image
//     keyframes, same reasoning a sandboxed document would have forced
//     anyway had this stayed there.
//   portrait=true -- /me/portrait, the self-portrait tool. No frame
//     history needed (an avatar isn't a sequence): Save just flattens the
//     current layers and POSTs the one image straight to /me/avatar.
//   post_target non-empty -- a post composer (/me/post/canvas, /feed/canvas,
//     /c/<slug>/canvas): the canvas itself IS the post's photo/video editor.
//     No frame history either; Save flattens the layers, pairs the image
//     with the caption textarea, and POSTs both as one multipart request
//     straight to post_target (/me/post, /feed, or /c/<slug>) -- the exact
//     same read_composer_post()-backed route a plain file-upload composer
//     already posts to, so no new backend ingestion path was needed for
//     this at all, only a richer way to produce the "media" file first.
// Everything else -- the whole brush/shape/layer/select/import engine -- is
// identical, built once here rather than forked into three copies that
// would drift apart.
std::string canvas_tool_page(const std::string& username, const std::string& err, bool portrait,
                             const std::string& post_target) {
    bool post_mode = !post_target.empty();
    // Reopening the profile-background tool used to always start from a
    // blank canvas, even when a drawing was already saved -- the only way
    // to "edit" it was to redraw from scratch. Looked up once here so both
    // the frame-count label and the actual pixel reload below can use it.
    std::size_t existing_frames = 0;
    bool existing_timelapse = false;
    if (!portrait && post_target.empty()) {
        if (auto owner = g_store.find_by_username(username)) {
            existing_frames = g_store.canvas_frames(owner->id).size();
            existing_timelapse = g_store.canvas_timelapse(owner->id);
        }
    }
    // A post drawing is shown at the post's full width, so its bitmap is 1280 x 960 (Brief 6 item 5: sharp on a
    // phone's 3x screen; the post path takes images up to SizeCaps::image_bytes, 10 MB, with no edge limit). PX is
    // bitmap pixels per pixel of the old 640 x 480 canvas: the script scales every size the person picks (brush,
    // shape, smudge, text, marquee) by it, so a size-8 brush looks as thick as before. /me/canvas stays 640 x 440:
    // its frames go through save_avatar, which refuses an edge over kAvatarMaxEdgePx (1024) or 2 MB.
    const int px = post_mode ? 2 : 1;
    int cw = portrait ? 400 : 640 * px;
    int ch = portrait ? 400 : (post_mode ? 480 * px : 440);
    std::string cws = std::to_string(cw), chs = std::to_string(ch);
    std::string aspect = cws + "/" + chs;
    // `err` is already-rendered HTML from flash_error(qs) at the call site
    // (frameimage/framelarge/framefull), not a raw code -- this function
    // doesn't need to know the code table, just where to put the result.
    std::string errHtml;
    if (!portrait && !post_mode) {
        errHtml = err;
    }
    // Icon buttons, not text labels: each of these already has a symbol
    // recognized from every other editor -- currentColor so they follow
    // the button's own text colour (and the light/dark theme) for free.
    const std::string kIconCopy =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"5.5\" y=\"5.5\" width=\"8\" height=\"8\" rx=\"1\"/>"
        "<path d=\"M3 10.5V3a1 1 0 0 1 1-1h7.5\"/></svg>";
    const std::string kIconCut =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><circle cx=\"4\" cy=\"4\" r=\"1.6\"/>"
        "<circle cx=\"4\" cy=\"12\" r=\"1.6\"/><path d=\"M5.2 5.2 13 13M5.2 10.8 13 3\"/></svg>";
    const std::string kIconPaste =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"3\" y=\"3\" width=\"10\" height=\"11.5\" rx=\"1\"/>"
        "<rect x=\"6\" y=\"1.3\" width=\"4\" height=\"2.4\" rx=\"0.6\"/></svg>";
    const std::string kIconCrop =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><path d=\"M4 1v10.5a1 1 0 0 0 1 1H15\"/>"
        "<path d=\"M12 15V4.5a1 1 0 0 0-1-1H1\"/></svg>";
    // Same icon on all three layer tabs, deliberately -- a layer tab's job
    // is "switch to this one" and "is this the active one" (shown by the
    // ghost/highlighted background), not "remember which number this is".
    const std::string kIconLayer =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.1\" aria-hidden=\"true\"><path d=\"M8 1 14 4.2 8 7.4 2 4.2Z\"/>"
        "<path d=\"M2 7.6 8 10.8 14 7.6\"/><path d=\"M2 11 8 14.2 14 11\"/></svg>";
    // The tool picker, same reasoning: eleven recognizable shapes read
    // faster than eleven words in a dropdown you have to open first.
    const std::string kIconPen =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M3 13l.6-2.8L10.6 3.2a1 1 0 0 1 1.4 0l.8.8a1 1 0 0 1 0 1.4"
        "L5.8 12.4 3 13Z\"/><path d=\"M9.3 4.5 11.5 6.7\"/></svg>";
    // A bare thick diagonal line read as just "a thicker Line tool", not
    // "marker" -- a recognizable pen body (the barrel) plus its own ink
    // mark reads as the tool, not the stroke it happens to draw.
    const std::string kIconMarker =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><rect x=\"2.3\" y=\"2.3\" width=\"4\" height=\"10.5\" rx=\"1\" "
        "transform=\"rotate(40 4.3 7.55)\"/><path d=\"M2.3 13.8h4\" stroke-width=\"2\" stroke-linecap=\"round\"/></svg>";
    const std::string kIconSpray =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><circle cx=\"5.5\" cy=\"9.5\" r=\"2.2\"/>"
        "<circle cx=\"11\" cy=\"3\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"13\" cy=\"6\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"9.5\" cy=\"3.5\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"11.5\" cy=\"8.5\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/></svg>";
    // A single wavy line just read as "squiggle," not "blend/drag" --
    // three parallel strokes fading out reads as a motion trail, the same
    // visual shorthand smudge/blur tools use elsewhere.
    const std::string kIconSmudge =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.4\" stroke-linecap=\"round\" aria-hidden=\"true\"><path d=\"M3.5 12.5 7.5 8.5\"/>"
        "<path d=\"M6 13.5 10 9.5\" opacity=\".6\"/><path d=\"M8.5 14.5 12.5 10.5\" opacity=\".3\"/></svg>";
    const std::string kIconEraser =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\">"
        "<rect x=\"3.3\" y=\"6.3\" width=\"9.4\" height=\"5.4\" rx=\"1\" transform=\"rotate(-28 8 9)\"/></svg>";
    const std::string kIconLine =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\"><path d=\"M2.5 13.5 13.5 2.5\"/></svg>";
    const std::string kIconRect =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"2.5\" y=\"3.5\" width=\"11\" height=\"9\" rx=\"1\"/></svg>";
    const std::string kIconCircle =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"8\" r=\"5.3\"/></svg>";
    // The old abstract polygon-plus-drip didn't read as a bucket at 16px --
    // a real trapezoid body with a rim and a single drip is the shape
    // paint-bucket icons actually use everywhere else.
    // The bucket's body and drip are filled with the actual selected
    // colour (kept in sync by ccolor's own input listener below), not a
    // static outline -- so the tool icon itself shows what it's about to
    // pour, the way a real paint bucket would.
    const std::string kIconBucket =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path id=\"bucketpaint\" fill=\"#1a1814\" "
        "d=\"M3 5h8.5l-1.1 6.3a1 1 0 0 1-1 .8H5.1a1 1 0 0 1-1-.8L3 5Z\"/>"
        "<path d=\"M2.5 5 4.3 2h5.4l1.8 3\"/>"
        "<circle id=\"bucketdrip\" cx=\"13\" cy=\"12.2\" r=\"1.2\" fill=\"#1a1814\" stroke=\"none\"/></svg>";
    const std::string kIconSelect =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-dasharray=\"2.3 2\" aria-hidden=\"true\">"
        "<rect x=\"2.5\" y=\"2.5\" width=\"11\" height=\"11\" rx=\"1\"/></svg>";
    const std::string kIconText =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\">"
        "<path d=\"M2.8 3.5h10.4\"/><path d=\"M8 3.5v9\"/></svg>";
    // The standard "highlight colour" glyph everywhere from Word to Google
    // Docs: a capital A with a solid colour bar underneath it.
    const std::string kIconHighlight =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M4 10.5 7 3h2l3 7.5\"/><path d=\"M5.1 8h5.8\"/>"
        "<rect x=\"2.5\" y=\"12.3\" width=\"11\" height=\"1.8\" fill=\"currentColor\" stroke=\"none\"/></svg>";
    // One dial glyph, reused at two sizes for the size/opacity wheels below
    // -- a bigger circle reads as "coarser steps" and a smaller one as
    // "finer steps" on its own, without needing a second icon design to
    // tell them apart (and risk being just as unclear as an abstract pair
    // would be).
    const std::string kIconDial =
        "<svg viewBox=\"0 0 16 16\" width=\"100%\" height=\"100%\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"8\" r=\"6.5\"/><path d=\"M8 2.5v3\"/></svg>";
    // The standard diagonal-dot resize grip every native textarea already
    // uses in its own bottom-right corner.
    const std::string kIconResizeGrip =
        "<svg viewBox=\"0 0 16 16\" width=\"100%\" height=\"100%\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<circle cx=\"12\" cy=\"4\" r=\"1.1\"/><circle cx=\"12\" cy=\"8\" r=\"1.1\"/><circle cx=\"12\" cy=\"12\" r=\"1.1\"/>"
        "<circle cx=\"8\" cy=\"8\" r=\"1.1\"/><circle cx=\"8\" cy=\"12\" r=\"1.1\"/><circle cx=\"4\" cy=\"12\" r=\"1.1\"/></svg>";
    // Glow/Mirror/Fill were the last checkbox-plus-label modifiers left --
    // converted to the same toggle-button pattern as Bold/Italic/Underline
    // now, for the same reason: faster to scan as a row of symbols than a
    // row of checkboxes each needing its own word read next to it.
    const std::string kIconGlow =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M8 1.5v2.4M8 12.1v2.4M1.5 8h2.4M12.1 8h2.4\"/>"
        "<path d=\"M3.8 3.8l1.7 1.7M10.5 10.5l1.7 1.7M12.2 3.8l-1.7 1.7M5.5 10.5l-1.7 1.7\"/>"
        "<circle cx=\"8\" cy=\"8\" r=\"1.8\"/></svg>";
    const std::string kIconMirror =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M8 1v14\" stroke-dasharray=\"2 1.6\"/>"
        "<path d=\"M4 5 2 8l2 3M12 5l2 3-2 3\"/></svg>";
    const std::string kIconFillShape =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<rect x=\"3\" y=\"3\" width=\"10\" height=\"10\" rx=\"1.5\"/></svg>";
    std::string title = portrait ? "Self-portrait" : (post_mode ? "Compose a post" : "Canvas");
    std::string intro = portrait
        ? "Draw your self-portrait with the same tools as the profile-background canvas &mdash; "
          "pen, shapes, layers, all of it."
        : post_mode
        ? "Draw, crop, layer, paste in a photo, or drop text on it &mdash; then post it, caption included."
        : "Draw, then save it as your profile background &mdash; a still image, "
          "or a loop of the drawing as it happened.";
    std::string html =
        "<div class=\"card canvascard\"><h1>"+title+"</h1>"
        "<p class=\"muted\">"+intro+"</p>"+errHtml+
        // Everything about "what am I about to draw with" lives in one
        // bounded panel -- tool, then that tool's own properties, then
        // (only for Text) that tool's extra properties -- separate from
        // the panel below it for "what can I do to the canvas as a
        // whole", and separate again from the canvas itself. Three
        // questions, three visually distinct answers, instead of one
        // long run of rows a reader has to parse to find which control
        // belongs to which question.
        // One frame around the whole workspace, not a separate panel
        // floating above the canvas: at desktop widths (see .canvas-frame
        // in assets/site.css) this becomes a real toolbox-and-canvas layout -- tools
        // in a column down the left, properties in a strip across the
        // top, canvas and its layer tabs filling the rest -- the same
        // wrap-the-canvas shape a real paint app uses, built as five grid
        // areas placed by CSS rather than by moving any of this markup;
        // below that width it's simply the original stacked column, no
        // media query applies at all.
        "<div class=\"card canvas-frame\">"
        // One toolbar above the drawing surface, four groups in order: tools | size, opacity, colour (and the Text
        // tool's own row) | layers | undo, redo, clipboard (Brandon 2026-11-11, Brief 4 item 7). It wraps at narrow widths.
        "<div class=\"ctoolbar\" role=\"toolbar\" aria-label=\"Drawing tools\">"
        // Three clusters, not one undifferentiated run of eleven icons:
        // freehand media, then shapes, then whole-canvas/utility tools --
        // each its own visual group with a hairline divider between,
        // read the same way a real paint app's toolbar groups by kind.
        "<div class=\"ctgroup ctools\" role=\"group\" aria-label=\"Tools\">"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"pen\" title=\"Pen\" "
        "style=\"padding:.5rem .55rem\">"+kIconPen+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"marker\" title=\"Marker\" "
        "style=\"padding:.5rem .55rem\">"+kIconMarker+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"spray\" title=\"Spray\" "
        "style=\"padding:.5rem .55rem\">"+kIconSpray+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"smudge\" title=\"Smudge\" "
        "style=\"padding:.5rem .55rem\">"+kIconSmudge+"</button>"
        // Eraser gets a colour of its own -- pink, not currentColor --
        // so it reads as visually distinct from the ink-laying tools
        // around it at a glance, not just by its shape.
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"eraser\" title=\"Eraser\" "
        "style=\"padding:.5rem .55rem;color:#ec6fa0\">"+kIconEraser+"</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"line\" title=\"Line\" "
        "style=\"padding:.5rem .55rem\">"+kIconLine+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"rect\" title=\"Rectangle\" "
        "style=\"padding:.5rem .55rem\">"+kIconRect+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"circle\" title=\"Circle\" "
        "style=\"padding:.5rem .55rem\">"+kIconCircle+"</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"bucket\" title=\"Paint bucket\" "
        "style=\"padding:.5rem .55rem\">"+kIconBucket+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"select\" title=\"Select\" "
        "style=\"padding:.5rem .55rem\">"+kIconSelect+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"text\" title=\"Text\" "
        "style=\"padding:.5rem .55rem\">"+kIconText+"</button>"
        "</div>"
        // The three property/modifier/text rows below share one wrapping
        // div, not separate grid placements -- multiple items placed at
        // the exact same grid-row/grid-column overlap each other rather
        // than stacking (that's what actually broke the first version of
        // this layout, live), so they need to be one single grid item
        // that stacks its own children by normal block flow internally.
        "<div class=\"ctgroup cprops\" role=\"group\" aria-label=\"Size, opacity and color\">"
        "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center;margin-bottom:.4rem\">"
        "<input type=\"color\" id=\"ccolor\" value=\"#1a1814\" aria-label=\"Color\" "
        "style=\"width:36px;min-height:36px;padding:2px;flex:0 0 auto\">"
        // Coarse/fine wheels instead of a single slider: drag one
        // vertically, or scroll it, and it nudges the number beside it --
        // the bigger circle moves in bigger steps, the smaller one in
        // smaller steps, same size-implies-precision logic as a real
        // jog/shuttle pair, so which is which never needs explaining.
        "<label>Size</label>"
        "<div class=\"row\" style=\"gap:.3rem;align-items:center\">"
        "<div class=\"wheel\" id=\"csizec\" data-target=\"csize\" data-step=\"5\" title=\"Size, coarse (drag or scroll, &plusmn;5)\" "
        "style=\"width:30px;height:30px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<div class=\"wheel\" id=\"csizef\" data-target=\"csize\" data-step=\"1\" title=\"Size, fine (drag or scroll, &plusmn;1)\" "
        "style=\"width:20px;height:20px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<input type=\"number\" id=\"csize\" min=\"1\" max=\"48\" value=\"8\" style=\"width:4rem\">"
        "</div>"
        "<label>Opacity</label>"
        "<div class=\"row\" style=\"gap:.3rem;align-items:center\">"
        "<div class=\"wheel\" id=\"copacityc\" data-target=\"copacity\" data-step=\"20\" "
        "title=\"Opacity, coarse (drag or scroll, &plusmn;20)\" "
        "style=\"width:30px;height:30px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<div class=\"wheel\" id=\"copacityf\" data-target=\"copacity\" data-step=\"5\" "
        "title=\"Opacity, fine (drag or scroll, &plusmn;5)\" "
        "style=\"width:20px;height:20px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<input type=\"number\" id=\"copacity\" min=\"5\" max=\"100\" value=\"100\" style=\"width:4rem\">"
        "</div>"
        // Glow/Mirror/Fill are stroke modifiers, not stroke properties,
        // but still belong in this same single strip -- a second row here
        // would make the wrap two rows thick instead of one.
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cglow\" data-on=\"0\" title=\"Glow\" "
        "style=\"padding:.5rem .55rem\">"+kIconGlow+"</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"csymmetry\" data-on=\"0\" title=\"Mirror\" "
        "style=\"padding:.5rem .55rem\">"+kIconMirror+"</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cfill\" data-on=\"0\" title=\"Fill shape\" "
        "style=\"padding:.5rem .55rem\">"+kIconFillShape+"</button>"
        "</div>"
        // Text tool controls -- font/weight/style/underline/background all
        // apply at commit time (see addTextBox), read the same way colour
        // and size already are for every other tool. Drag the Text tool
        // itself on the canvas to size the box: vertical drag sets the
        // font size, horizontal sets the field width text wraps within.
        // Only meaningful for one of eleven tools, so this whole row stays
        // hidden (see refreshToolBtns) until the Text tool is actually
        // selected, instead of cluttering every other tool's controls.
        // Bold/Italic/Underline are their own established symbols --
        // literally the styled B/I/U every text editor uses -- so they're
        // toggle buttons showing that styling directly, not checkboxes
        // with a redundant word next to them; Background gets the
        // standard highlight-colour glyph for the same reason.
        "<div class=\"row\" id=\"ctextrow\" hidden "
        "style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center;margin-bottom:.6rem\">"
        "<select id=\"cfont\" style=\"width:auto;min-height:36px\">"
        "<option value=\"sans-serif\">Sans-serif</option>"
        "<option value=\"serif\">Serif</option>"
        "<option value=\"monospace\">Monospace</option>"
        "<option value=\"cursive\">Cursive</option>"
        "</select>"
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cbold\" data-on=\"0\" title=\"Bold\" "
        "style=\"padding:.5rem .75rem;font-weight:bold\">B</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"citalic\" data-on=\"0\" title=\"Italic\" "
        "style=\"padding:.5rem .75rem;font-style:italic\">I</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cunderline\" data-on=\"0\" title=\"Underline\" "
        "style=\"padding:.5rem .75rem;text-decoration:underline\">U</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"ctextbg\" data-on=\"0\" title=\"Text background\" "
        "style=\"padding:.5rem .6rem\">"+kIconHighlight+"</button>"
        "<input type=\"color\" id=\"ctextbgcolor\" value=\"#ffffff\" aria-label=\"Text background color\" "
        "style=\"width:36px;min-height:36px;padding:2px;flex:0 0 auto\">"
        "</div>"
        "</div>"
        "</div>"
        // the layer tabs, each over its own opacity slider (Brief 4 item 7: in the toolbar, not beside the canvas)
        "<div class=\"ctgroup clayers\" role=\"group\" aria-label=\"Layers\">"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"0\" title=\"Switch to layer 1\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"0\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 1 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"1\" title=\"Switch to layer 2\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"1\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 2 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"2\" title=\"Switch to layer 3\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"2\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 3 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "</div>"
        // Undo/redo/clear and the selection actions (copy/cut/paste/crop)
        // complete the frame with a third edge along the canvas's bottom,
        // one row thick like the other two -- both groups act on the
        // canvas as a whole rather than being "what the next stroke looks
        // like", which is why they're grouped apart from the tool/
        // properties edges above rather than mixed into them.
        "<div class=\"ctgroup cedit\" role=\"group\" aria-label=\"Undo and selection\">"
        "<button type=\"button\" class=\"ghost\" id=\"cundo\" title=\"Undo\">&#8617;</button>"
        "<button type=\"button\" class=\"ghost\" id=\"credo\" title=\"Redo\">&#8618;</button>"
        "<button type=\"button\" class=\"ghost\" id=\"cclear\">Clear layer</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        // Drag a rectangle with the Select tool first -- these four
        // buttons all act on that same selection (copy/cut sample it,
        // paste drops the last-copied piece at your next click, crop
        // trims the whole canvas down to it). Icons, not labels: each of
        // these already has a symbol everyone recognizes from every other
        // editor (clipboard stack, scissors, crop corners) -- a text
        // label would just be slower to scan than the glyph itself.
        "<button type=\"button\" class=\"ghost\" id=\"ccopy\" title=\"Copy selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCopy+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"ccut\" title=\"Cut selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCut+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"cpaste\" title=\"Paste\" "
        "style=\"padding:.5rem .55rem\">"+kIconPaste+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"ccrop\" title=\"Apply crop to selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCrop+"</button>"
        "</div>"
        "</div>" // end of the toolbar
        // Canvas, with its layer tabs on the right, as the fourth and
        // final area of .canvas-frame -- filling the rest of the frame
        // beside/under the tool column and properties strip above. No
        // numbers on the layer tabs themselves: switching to one and
        // seeing which is active (the highlighted background) is all a
        // tab needs to communicate, and all three do the same job -- a
        // number would just be one more thing to read, not new
        // information. Each still gets its own opacity slider, a real
        // per-layer property applied both live (CSS opacity on that
        // layer's own <canvas>) and in flatten() so a saved/posted image
        // matches what's on screen.
        "<div class=\"ccanvasrow\" style=\"display:flex;gap:.5rem;align-items:flex-start\">"
        "<div id=\"ccanvaswrap\" style=\"position:relative;flex:1;min-width:0;"
        "aspect-ratio:"+aspect+";background:#fff;border:1px solid var(--rule);"
        "border-radius:var(--rsm);overflow:hidden;touch-action:none\">"
        "<canvas id=\"clayer0\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"clayer1\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"clayer2\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"coverlay\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%;cursor:crosshair\"></canvas>"
        "<div id=\"cresizehandle\" title=\"Drag to resize the canvas\" "
        "style=\"position:absolute;right:0;bottom:0;width:16px;height:16px;padding:2px;"
        "cursor:nwse-resize;z-index:11;touch-action:none;color:var(--stone)\">"+kIconResizeGrip+"</div>"
        "</div>"
        "</div>"
        "</div>";
    if (!portrait) {
        html +=
        "<div class=\"card\">"
        "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center\">"
        "<label class=\"ghost\" style=\"cursor:pointer;padding:.3rem .6rem;border-radius:var(--rsm)\">"
        "Import image/video<input type=\"file\" id=\"cimport\" accept=\"image/*,video/*\" "
        "style=\"display:none\"></label>"
        "</div>"
        "<p class=\"muted\" style=\"margin:.5rem 0 0\">Ctrl+V also pastes an image straight from your "
        "OS clipboard onto the active layer.</p>"
        "</div>";
    } else {
        html += "<p class=\"muted\">Self-portraits are drawn here. Photo upload and paste are off on this page.</p>";
    }
        // A starting texture under whatever gets drawn, the way real paper
        // has a grain before any ink touches it -- four options, Plain
        // (today's blank default) among them, exactly one active at a
        // time like the tool row's own .toolbtn pattern. Lives on
        // #ccanvaswrap's own CSS background (layers stay untouched, fully
        // transparent as always), so it's visible while editing and
        // switchable freely, but NOT yet part of flatten()'s output: a
        // save/post still composites the transparent layers onto a plain
        // white backing, same as before this control existed. Baking a
        // choice into the actual saved pixels is a separate, larger change
        // (drawing real texture onto layer 0 at canvas-open time) --
        // this is deliberately just the live, switchable preview of the idea.
        "<div class=\"row\" style=\"margin-bottom:.5rem;align-items:center;gap:.4rem;flex-wrap:wrap\">"
        "<label class=\"muted\" style=\"font-size:.8rem\">Canvas background</label>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"hatch\">Hatch</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"plain\" data-on=\"1\">Plain</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"wash\">Wash</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"ruled\">Ruled</button>"
        "</div>";
    if (!portrait && !post_mode) {
        html +=
            "<p class=\"muted\" id=\"cframecount\" style=\"margin-top:.6rem\">"+std::to_string(existing_frames)+
            "/24 frames captured</p>"
            "<label style=\"display:block;margin-top:.4rem\"><input type=\"checkbox\" id=\"ctimelapse\""+
            (existing_timelapse ? " checked" : "")+"> "
            "Loop the drawing as my background instead of a still image</label>";
    }
    if (post_mode) {
        html +=
            "<label style=\"display:block;margin-top:.6rem\">Caption</label>"
            "<textarea id=\"cpcaption\" maxlength=\"2000\" placeholder=\"What's on your mind?\"></textarea>";
    }
    std::string saveLabel = portrait ? "Save as self-portrait" : (post_mode ? "Post" : "Save to profile");
    html +=
        "<div class=\"row\" style=\"margin-top:.6rem\">"
        "<button type=\"button\" id=\"csave\">"+saveLabel+"</button>"
        "</div>"
        "<p class=\"muted\" id=\"cstatus\"></p></div>"
        ;
    // The drawing engine is assets/canvas.js (moved out of this file 2026-11-13, code moved, not rewritten); this page
    // passes its own values in. Usernames and post targets are [A-Za-z0-9._/-] only, so JSON-escaped text cannot end
    // the script.
    html += "<script>window.LG_CANVAS={\"px\":" + std::to_string(px) + ",\"frames\":" + std::to_string(existing_frames) +
            ",\"user\":\"" + json_escape(username) + "\",\"mode\":\"" + (portrait ? "portrait" : (post_mode ? "post" : "frames")) +
            "\",\"postTarget\":\"" + json_escape(post_target) + "\"};</script><script src=\"" + asset_url("canvas.js") + "\"></script>";
    // No droplet on the drawing tool (2026-11-15): the canvas fills a phone's screen and the touchpad would sit on it,
    // taking the strokes. wrap() leaves out the drop on a page that carries this marker.
    html += "<!--lg:no-drop-->";
    return html;
}

}  // namespace liquidgov::site
