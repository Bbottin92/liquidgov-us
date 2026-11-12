#include "site/features/canvas/canvas.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"

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
        "<script>(function(){"
        "var LAYN=3,layers=[],PX="+std::to_string(px)+";"
        "for(var li=0;li<LAYN;li++){"
        "var lc=document.getElementById('clayer'+li);"
        "layers.push({cv:lc,ctx:lc.getContext('2d'),undo:[],redo:[],opacity:1});"
        "}"
        + (existing_frames == 0 ? "" : std::string(
        // Reopening with a drawing already saved: paint its last frame
        // onto the bottom layer before anything else runs, so editing
        // continues the existing picture instead of starting blank. The
        // eventual Save/first-capture still clears old frame history on
        // the server (see captureFrame's clearedOnce below) -- flatten()
        // by then already includes these loaded pixels plus whatever new
        // marks were added, so nothing already drawn is lost.
        "(function(){"
        "var img=new Image();"
        "img.onload=function(){layers[0].ctx.drawImage(img,0,0,layers[0].cv.width,layers[0].cv.height);};"
        "img.src='/canvas-frame/'+"+js_string_literal(username)+"+'/"+std::to_string(existing_frames-1)+"';"
        "})();"
        )) +
        "document.querySelectorAll('.layeropacity').forEach(function(s){"
        "s.addEventListener('input',function(){"
        "var L=layers[Number(s.dataset.layer)];"
        "L.opacity=Number(s.value)/100;L.cv.style.opacity=L.opacity;"
        "});"
        "});"
        "var CBG={"
        "plain:'#fff',"
        "wash:\"radial-gradient(circle at 30% 35%,rgba(201,168,106,.35) 0%,rgba(201,168,106,0) 65%),\""
        "+\"radial-gradient(circle at 75% 70%,rgba(162,130,59,.28) 0%,rgba(162,130,59,0) 55%),\""
        "+\"radial-gradient(circle at 60% 15%,rgba(110,59,162,.10) 0%,rgba(110,59,162,0) 45%),#f7f1e4\","
        "hatch:\"repeating-linear-gradient(45deg,rgba(26,24,20,.16) 0,rgba(26,24,20,.16) 1px,transparent 1px,transparent 9px),#f7f1e4\","
        "ruled:\"repeating-linear-gradient(to bottom,transparent 0,transparent 17px,rgba(26,24,20,.22) 17px,rgba(26,24,20,.22) 18px),#f7f1e4\""
        "};"
        "var cbgBtns=document.querySelectorAll('.cbgbtn');"
        "cbgBtns.forEach(function(b){"
        "b.addEventListener('click',function(){"
        "cbgBtns.forEach(function(o){o.dataset.on='0';o.style.background='';});"
        "b.dataset.on='1';b.style.background='var(--rule)';"
        "document.getElementById('ccanvaswrap').style.background=CBG[b.dataset.bg];"
        "});"
        "});"
        "cbgBtns.forEach(function(b){if(b.dataset.on==='1')b.style.background='var(--rule)';});"
        "var activeLayer=0;"
        "function acv(){return layers[activeLayer].cv;}"
        "function actx(){return layers[activeLayer].ctx;}"
        "var overlay=document.getElementById('coverlay'),octx=overlay.getContext('2d');"
        "function clearOverlay(){octx.clearRect(0,0,overlay.width,overlay.height);}"
        "var drawing=false,lastX=0,lastY=0,shapeStart=null;"
        "function snap(){"
        "var L=layers[activeLayer];"
        "L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "if(L.undo.length>20)L.undo.shift();"
        "L.redo=[];"
        "}"
        "function pos(e){"
        "var r=overlay.getBoundingClientRect();var p=e.touches?e.touches[0]:e;"
        "return{x:(p.clientX-r.left)*overlay.width/r.width,y:(p.clientY-r.top)*overlay.height/r.height};"
        "}"
        "var curTool='pen';"
        "function toolVal(){return curTool;}"
        // The bucket icon's own fill tracks whatever colour is loaded,
        // live -- both on page load (the input's own default) and on
        // every change, so the tool always shows what it's about to pour.
        "function syncBucketColor(){"
        "var c=document.getElementById('ccolor').value;"
        "var p=document.getElementById('bucketpaint'),d=document.getElementById('bucketdrip');"
        "if(p)p.setAttribute('fill',c);if(d)d.setAttribute('fill',c);"
        "}"
        "document.getElementById('ccolor').addEventListener('input',syncBucketColor);"
        "syncBucketColor();"
        "var toolBtns=document.querySelectorAll('.toolbtn');"
        "function refreshToolBtns(){"
        "toolBtns.forEach(function(b){b.style.background=b.dataset.tool===curTool?'var(--rule)':'';});"
        // .row{display:flex} outranks the browser's default [hidden]{display:
        // none} in the cascade (same specificity, .row comes later in the
        // stylesheet), so the hidden attribute alone silently does nothing
        // on an element that also has class="row" -- toggling style.display
        // directly sidesteps that instead of fighting the cascade.
        "document.getElementById('ctextrow').style.display=curTool==='text'?'flex':'none';"
        "}"
        "toolBtns.forEach(function(b){"
        "b.addEventListener('click',function(){curTool=b.dataset.tool;refreshToolBtns();});"
        "});"
        "refreshToolBtns();"
        "document.querySelectorAll('.fmtbtn').forEach(function(b){"
        "b.addEventListener('click',function(){"
        "var on=b.dataset.on==='1';b.dataset.on=on?'0':'1';b.style.background=on?'':'var(--rule)';"
        "});"
        "});"
        "function drawSeg(ax,ay,bx,by){"
        "var b=toolVal(),color=document.getElementById('ccolor').value;"
        "var size=Number(document.getElementById('csize').value)*PX;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var glow=document.getElementById('cglow').dataset.on==='1';"
        "var cx=actx();"
        "cx.save();"
        "if(glow){cx.shadowColor=color;cx.shadowBlur=size*1.4;}"
        "if(b==='eraser'){"
        "cx.globalCompositeOperation='destination-out';cx.globalAlpha=1;"
        "cx.lineCap='round';cx.lineJoin='round';cx.lineWidth=size*2;"
        "cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();"
        "}else if(b==='spray'){"
        "cx.globalAlpha=opacity;cx.fillStyle=color;"
        "var n=Math.max(6,Math.floor(size));"
        "for(var i=0;i<n;i++){"
        "var ang=Math.random()*Math.PI*2,rad=Math.random()*size;"
        "cx.beginPath();cx.arc(bx+Math.cos(ang)*rad,by+Math.sin(ang)*rad,PX,0,Math.PI*2);cx.fill();"
        "}"
        "}else{"
        "cx.lineCap='round';cx.lineJoin='round';cx.strokeStyle=color;"
        "if(b==='marker'){cx.lineWidth=size*1.6;cx.globalAlpha=Math.min(opacity,.45);}"
        "else{cx.lineWidth=size;cx.globalAlpha=opacity;}"
        "cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();"
        "}"
        "cx.restore();"
        "}"
        "function strokeAt(x0,y0,x1,y1){"
        "drawSeg(x0,y0,x1,y1);"
        "if(document.getElementById('csymmetry').dataset.on==='1')"
        "drawSeg(acv().width-x0,y0,acv().width-x1,y1);"
        "}"
        // Shared by the live drag preview (drawn onto the overlay) and the
        // final commit (drawn onto the active layer) -- same geometry
        // either way, only the target context differs.
        "function drawShape(x0,y0,x1,y1,targetCtx){"
        "var t=toolVal(),color=document.getElementById('ccolor').value;"
        "var size=Number(document.getElementById('csize').value)*PX;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var fill=document.getElementById('cfill').dataset.on==='1';"
        "var glow=document.getElementById('cglow').dataset.on==='1';"
        "targetCtx.save();"
        "targetCtx.globalAlpha=opacity;targetCtx.strokeStyle=color;targetCtx.fillStyle=color;"
        "targetCtx.lineWidth=size;targetCtx.lineCap='round';targetCtx.lineJoin='round';"
        "if(glow){targetCtx.shadowColor=color;targetCtx.shadowBlur=size*1.4;}"
        "if(t==='line'){"
        "targetCtx.beginPath();targetCtx.moveTo(x0,y0);targetCtx.lineTo(x1,y1);targetCtx.stroke();"
        "}else if(t==='rect'){"
        "var rx=Math.min(x0,x1),ry=Math.min(y0,y1),rw=Math.abs(x1-x0),rh=Math.abs(y1-y0);"
        "if(fill)targetCtx.fillRect(rx,ry,rw,rh);else targetCtx.strokeRect(rx,ry,rw,rh);"
        "}else if(t==='circle'){"
        "var mx=(x0+x1)/2,my=(y0+y1)/2,rxr=Math.abs(x1-x0)/2,ryr=Math.abs(y1-y0)/2;"
        "targetCtx.beginPath();targetCtx.ellipse(mx,my,rxr,ryr,0,0,Math.PI*2);"
        "if(fill)targetCtx.fill();else targetCtx.stroke();"
        "}"
        "targetCtx.restore();"
        "}"
        // Iterative 4-way flood fill on the active layer only, with a small
        // colour-distance tolerance so anti-aliased stroke edges don't
        // leave a ring of unfilled pixels around the bucket click.
        "function floodFill(fx,fy){"
        "var c=acv(),cx=actx();fx=Math.floor(fx);fy=Math.floor(fy);"
        "if(fx<0||fy<0||fx>=c.width||fy>=c.height)return;"
        "var img=cx.getImageData(0,0,c.width,c.height),data=img.data,w=c.width,h=c.height;"
        "var startI=(fy*w+fx)*4;"
        "var tr=data[startI],tg=data[startI+1],tb=data[startI+2],ta=data[startI+3];"
        "var hex=document.getElementById('ccolor').value;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var fr=parseInt(hex.slice(1,3),16),fg=parseInt(hex.slice(3,5),16),fb=parseInt(hex.slice(5,7),16);"
        "var fa=Math.round(opacity*255);"
        "if(tr===fr&&tg===fg&&tb===fb&&ta===fa)return;"
        "var tol=32,tol2=tol*tol*4;"
        "function matches(i){"
        "var dr=data[i]-tr,dg=data[i+1]-tg,db=data[i+2]-tb,da=data[i+3]-ta;"
        "return dr*dr+dg*dg+db*db+da*da<=tol2;"
        "}"
        "var visited=new Uint8Array(w*h);visited[fy*w+fx]=1;"
        "var stack=[startI];"
        "while(stack.length){"
        "var i=stack.pop();data[i]=fr;data[i+1]=fg;data[i+2]=fb;data[i+3]=fa;"
        "var px=(i/4)|0,x=px%w,y=(px/w)|0;"
        "var nbrs=[[x-1,y],[x+1,y],[x,y-1],[x,y+1]];"
        "for(var k=0;k<4;k++){"
        "var nx=nbrs[k][0],ny=nbrs[k][1];"
        "if(nx<0||ny<0||nx>=w||ny>=h)continue;"
        "var vi=ny*w+nx;if(visited[vi])continue;"
        "var ni=vi*4;"
        "if(matches(ni)){visited[vi]=1;stack.push(ni);}"
        "}"
        "}"
        "cx.putImageData(img,0,0);"
        "}"
        // Smudge: sample a small patch of the active layer around the
        // stroke's previous point and re-draw it, partially transparent,
        // at the new point -- dragging existing pixels along the path
        // instead of laying down a fresh colour.
        "var smudgeBuf=document.createElement('canvas');"
        "function smudgeAt(x,y){"
        "var cx=actx(),c=acv();"
        "var r=Math.max(4,Number(document.getElementById('csize').value))*PX;"
        "var sx=Math.max(0,Math.floor(lastX-r)),sy=Math.max(0,Math.floor(lastY-r));"
        "var sw=Math.min(c.width-sx,r*2),sh=Math.min(c.height-sy,r*2);"
        "if(sw<=0||sh<=0)return;"
        "smudgeBuf.width=sw;smudgeBuf.height=sh;"
        "var sctx=smudgeBuf.getContext('2d');sctx.clearRect(0,0,sw,sh);"
        "sctx.drawImage(c,sx,sy,sw,sh,0,0,sw,sh);"
        "cx.save();cx.globalAlpha=.45;"
        "cx.drawImage(smudgeBuf,0,0,sw,sh,x-r,y-r,sw,sh);"
        "cx.restore();"
        "}"
        // Select: drag a marquee (no pixel change of its own -- just
        // records selRect and draws a dashed outline). Copy/Cut sample
        // that rectangle from the active layer into an internal clip
        // canvas; Paste arms a "next click places it" mode rather than
        // dropping it at a fixed spot, since a canvas has no cursor to
        // paste "at" the way a text editor does.
        "var selRect=null,clip=null,pastePending=false;"
        "function rectFrom(a,b){return{x:Math.min(a.x,b.x),y:Math.min(a.y,b.y),w:Math.abs(b.x-a.x),h:Math.abs(b.y-a.y)};}"
        "function drawMarquee(r){octx.save();octx.strokeStyle='#000';octx.lineWidth=PX;octx.setLineDash([5*PX,4*PX]);"
        "octx.strokeRect(Math.round(r.x)+.5,Math.round(r.y)+.5,Math.round(r.w),Math.round(r.h));octx.restore();}"
        "function copySel(){"
        "if(!selRect||selRect.w<1||selRect.h<1)return;"
        "var c=document.createElement('canvas');c.width=Math.round(selRect.w);c.height=Math.round(selRect.h);"
        "c.getContext('2d').drawImage(acv(),selRect.x,selRect.y,c.width,c.height,0,0,c.width,c.height);"
        "clip=c;"
        "}"
        "document.getElementById('ccopy').addEventListener('click',copySel);"
        "document.getElementById('ccut').addEventListener('click',function(){"
        "if(!selRect||selRect.w<1||selRect.h<1)return;"
        "copySel();snap();actx().clearRect(selRect.x,selRect.y,selRect.w,selRect.h);captureFrame(false);"
        "});"
        "document.getElementById('cpaste').addEventListener('click',function(){if(clip)pastePending=true;});"
        "function placeClipboardAt(x,y){"
        "if(!clip)return;snap();actx().drawImage(clip,x-clip.width/2,y-clip.height/2);captureFrame(false);"
        "}"
        // Crop trims every layer's canvas down to the selection (sampled
        // BEFORE each layer's own canvas is resized, since resizing a
        // <canvas> element's width/height attribute always clears it).
        // Resize instead scales each layer's whole existing content into
        // a new canvas size -- no selection involved.
        "function resizeAllLayers(nw,nh,sampleRect){"
        "layers.forEach(function(L){"
        "var tmp=document.createElement('canvas');"
        "if(sampleRect){"
        "tmp.width=nw;tmp.height=nh;"
        "tmp.getContext('2d').drawImage(L.cv,sampleRect.x,sampleRect.y,nw,nh,0,0,nw,nh);"
        "}else{"
        "tmp.width=L.cv.width;tmp.height=L.cv.height;tmp.getContext('2d').drawImage(L.cv,0,0);"
        "}"
        "L.cv.width=nw;L.cv.height=nh;"
        "if(sampleRect)L.ctx.drawImage(tmp,0,0);else L.ctx.drawImage(tmp,0,0,tmp.width,tmp.height,0,0,nw,nh);"
        "L.undo=[];L.redo=[];"
        "});"
        "overlay.width=nw;overlay.height=nh;"
        "var wrap=document.getElementById('ccanvaswrap');"
        "wrap.style.maxWidth=(nw/PX)+'px';wrap.style.aspectRatio=nw+'/'+nh;"
        "selRect=null;clearOverlay();captureFrame(false);"
        "}"
        "document.getElementById('ccrop').addEventListener('click',function(){"
        "if(!selRect||selRect.w<2||selRect.h<2)return;"
        "resizeAllLayers(Math.round(selRect.w),Math.round(selRect.h),"
        "{x:Math.round(selRect.x),y:Math.round(selRect.y)});"
        "});"
        // Drag the corner grip to resize the canvas -- live CSS-only
        // preview while dragging (cheap: just the wrap's own box, the
        // actual bitmaps untouched so the existing drawing visibly
        // stretches to match, same as dragging an image placeholder in
        // any editor), then the real per-layer resize commits once on
        // release via the same resizeAllLayers() the old numeric-input
        // form used.
        "(function(){"
        "var handle=document.getElementById('cresizehandle'),wrap=document.getElementById('ccanvaswrap');"
        "var dragging=false,startX=0,startY=0,startCw=0,startCh=0,cssToCanvas=1;"
        "handle.addEventListener('pointerdown',function(e){"
        "dragging=true;startX=e.clientX;startY=e.clientY;"
        "var r=wrap.getBoundingClientRect();"
        "startCw=r.width;startCh=r.height;cssToCanvas=acv().width/r.width;"
        "handle.setPointerCapture(e.pointerId);e.preventDefault();e.stopPropagation();"
        "});"
        "handle.addEventListener('pointermove',function(e){"
        "if(!dragging)return;"
        "var nw=Math.max(40,startCw+(e.clientX-startX));"
        "var nh=Math.max(40,startCh+(e.clientY-startY));"
        "wrap.style.maxWidth=nw+'px';wrap.style.aspectRatio=nw+'/'+nh;"
        "e.preventDefault();e.stopPropagation();"
        "});"
        "handle.addEventListener('pointerup',function(e){"
        "if(!dragging)return;dragging=false;"
        "var nw=Math.max(16,Math.min(2000,Math.round((startCw+(e.clientX-startX))*cssToCanvas)));"
        "var nh=Math.max(16,Math.min(2000,Math.round((startCh+(e.clientY-startY))*cssToCanvas)));"
        "resizeAllLayers(nw,nh,null);"
        "e.preventDefault();e.stopPropagation();"
        "});"
        "})();"
        // Coarse/fine wheels: drag vertically or scroll over one to nudge
        // its target number input by its own step, clamped to that
        // input's own min/max. A little rotation on each notch is purely
        // cosmetic feedback that the wheel actually registered the input.
        "document.querySelectorAll('.wheel').forEach(function(w){"
        "var target=document.getElementById(w.dataset.target),step=Number(w.dataset.step),rotation=0;"
        "function apply(dir){"
        "var min=Number(target.min)||0,max=Number(target.max)||9999;"
        "target.value=Math.max(min,Math.min(max,Number(target.value)+dir*step));"
        "rotation+=dir*20;w.style.transform='rotate('+rotation+'deg)';"
        "}"
        "w.addEventListener('wheel',function(e){e.preventDefault();apply(e.deltaY<0?1:-1);},{passive:false});"
        "var dragging=false,lastY=0;"
        "w.addEventListener('pointerdown',function(e){"
        "dragging=true;lastY=e.clientY;w.setPointerCapture(e.pointerId);e.preventDefault();"
        "});"
        "w.addEventListener('pointermove',function(e){"
        "if(!dragging)return;"
        "var dy=lastY-e.clientY;"
        "if(Math.abs(dy)>=6){apply(dy>0?1:-1);lastY=e.clientY;}"
        "});"
        "w.addEventListener('pointerup',function(){dragging=false;});"
        "});"
        // Text: drag the Text tool on the canvas -- vertical distance
        // sets the font size, horizontal sets the field width the text
        // wraps within (a plain click falls back to a sensible default
        // box instead of doing nothing). Opens a real <textarea> over the
        // canvas, styled live from the font/bold/italic/underline
        // controls, so what you see while typing already looks like what
        // commit() is about to bake onto the active layer as pixels
        // (word-wrapped to the box width, underline drawn by hand since
        // canvas text has no native underline, background filled first if
        // enabled) -- blur commits, Escape cancels without committing.
        "function addTextBox(x,y,boxW,fontPx){"
        "var r=overlay.getBoundingClientRect(),scale=r.width/overlay.width;"
        "var ta=document.createElement('textarea');"
        "ta.placeholder='Text';"
        "var bold=document.getElementById('cbold').dataset.on==='1';"
        "var italic=document.getElementById('citalic').dataset.on==='1';"
        "var underline=document.getElementById('cunderline').dataset.on==='1';"
        "var fam=document.getElementById('cfont').value;"
        "var color=document.getElementById('ccolor').value;"
        // width/min-height:auto overrides the site-wide
        // input,textarea{width:100%;min-height:44px} rule (meant for real
        // form fields), which would otherwise stretch this floating box
        // across the whole canvas -- the same class of bug already hit
        // input[type=color]/[type=checkbox] once each.
        "ta.style.cssText='position:absolute;z-index:10;width:auto;min-height:0;resize:both;overflow:auto;'+"
        "'padding:2px;box-sizing:border-box;background:rgba(255,255,255,.85);'+"
        "'border:1px dashed '+color+';color:'+color+';font-family:'+fam+';'+"
        "'font-size:'+(fontPx*scale)+'px;'+(bold?'font-weight:bold;':'')+(italic?'font-style:italic;':'')+"
        "(underline?'text-decoration:underline;':'');"
        "ta.style.left=(x*scale)+'px';ta.style.top=(y*scale)+'px';"
        "ta.style.width=(boxW*scale)+'px';ta.style.height=Math.max(fontPx*1.6,40*PX)*scale+'px';"
        "document.getElementById('ccanvaswrap').appendChild(ta);"
        "ta.focus();"
        "var done=false;"
        "function commit(){"
        "if(done)return;done=true;"
        "var val=ta.value;ta.remove();"
        "if(!val)return;"
        "snap();var cx=actx();cx.save();"
        "cx.font=(italic?'italic ':'')+(bold?'bold ':'')+fontPx+'px '+fam;"
        "cx.textBaseline='top';"
        "var lineH=fontPx*1.25,lines=[];"
        "val.split('\\n').forEach(function(paragraph){"
        "var words=paragraph.split(' '),cur='';"
        "words.forEach(function(w){"
        "var test=cur?cur+' '+w:w;"
        "if(cur&&cx.measureText(test).width>boxW){lines.push(cur);cur=w;}else{cur=test;}"
        "});"
        "lines.push(cur);"
        "});"
        "if(document.getElementById('ctextbg').dataset.on==='1'){"
        "cx.fillStyle=document.getElementById('ctextbgcolor').value;"
        "cx.fillRect(x,y,boxW,lines.length*lineH);"
        "}"
        "cx.fillStyle=color;"
        "lines.forEach(function(line,i){"
        "var ly=y+i*lineH;cx.fillText(line,x,ly);"
        "if(underline){"
        "var w=cx.measureText(line).width;"
        "cx.fillRect(x,ly+fontPx*.92,w,Math.max(1,fontPx*.06));"
        "}"
        "});"
        "cx.restore();"
        "captureFrame(false);"
        "}"
        "ta.addEventListener('keydown',function(e){if(e.key==='Escape'){done=true;ta.remove();}});"
        "ta.addEventListener('blur',commit);"
        "}"
        // Paste from the OS clipboard (e.g. a screenshot copied outside
        // the browser) -- separate from the internal Copy/Cut/Paste
        // above, which only ever moves pixels around inside this canvas.
        "document.addEventListener('paste',function(e){"
        + std::string(portrait ? "return;" : "") +
        "var items=(e.clipboardData||{}).items||[];"
        "for(var i=0;i<items.length;i++){"
        "if(items[i].type.indexOf('image')!==0)continue;"
        "var file=items[i].getAsFile();if(!file)continue;"
        "var url=URL.createObjectURL(file),img=new Image();"
        "img.onload=function(){"
        "snap();var cvw=acv().width,cvh=acv().height;"
        "var scale=Math.min(cvw/img.width,cvh/img.height,1);"
        "var dw=img.width*scale,dh=img.height*scale;"
        "actx().drawImage(img,(cvw-dw)/2,(cvh-dh)/2,dw,dh);"
        "captureFrame(false);URL.revokeObjectURL(url);"
        "};"
        "img.src=url;e.preventDefault();break;"
        "}"
        "});"
        "overlay.addEventListener('pointerdown',function(e){"
        "var t=toolVal(),p=pos(e);"
        "if(pastePending){pastePending=false;placeClipboardAt(p.x,p.y);e.preventDefault();return;}"
        "if(t==='text'||t==='select'){shapeStart=p;e.preventDefault();return;}"
        "if(t==='bucket'){snap();floodFill(p.x,p.y);captureFrame(false);e.preventDefault();return;}"
        "if(t==='line'||t==='rect'||t==='circle'){snap();shapeStart=p;e.preventDefault();return;}"
        "snap();drawing=true;lastX=p.x;lastY=p.y;"
        "if(t!=='smudge')strokeAt(p.x,p.y,p.x+.1,p.y+.1);"
        "e.preventDefault();"
        "});"
        "overlay.addEventListener('pointermove',function(e){"
        "var t=toolVal(),p=pos(e);"
        "if(shapeStart&&(t==='select'||t==='text')){clearOverlay();drawMarquee(rectFrom(shapeStart,p));e.preventDefault();return;}"
        "if(shapeStart){clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,octx);e.preventDefault();return;}"
        "if(!drawing)return;"
        "if(t==='smudge')smudgeAt(p.x,p.y);else strokeAt(lastX,lastY,p.x,p.y);"
        "lastX=p.x;lastY=p.y;e.preventDefault();"
        "});"
        "window.addEventListener('pointerup',function(e){"
        "if(shapeStart&&toolVal()==='select'){"
        "var p=pos(e);var r=rectFrom(shapeStart,p);clearOverlay();"
        "if(r.w>=2&&r.h>=2){selRect=r;drawMarquee(r);}"
        "shapeStart=null;return;"
        "}"
        "if(shapeStart&&toolVal()==='text'){"
        "var p=pos(e);var r=rectFrom(shapeStart,p);var start=shapeStart;clearOverlay();shapeStart=null;"
        // A near-zero drag (effectively just a click) falls back to a
        // sensible default box instead of creating an unusable sliver.
        "var fontPx=r.h>=6*PX?Math.round(r.h):24*PX;"
        "var boxW=r.w>=20*PX?Math.round(r.w):200*PX;"
        "var tx=r.w>=6*PX?r.x:start.x,ty=r.h>=6*PX?r.y:start.y;"
        "addTextBox(tx,ty,boxW,fontPx);"
        "return;"
        "}"
        "if(shapeStart){"
        "var p=pos(e);clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,actx());"
        "shapeStart=null;captureFrame(false);return;"
        "}"
        "if(!drawing)return;drawing=false;captureFrame(false);"
        "});"
        "var layerBtns=document.querySelectorAll('.layerbtn');"
        "function refreshLayerBtns(){"
        "layerBtns.forEach(function(b){"
        "b.style.background=Number(b.dataset.layer)===activeLayer?'var(--rule)':'';"
        "});"
        "}"
        "layerBtns.forEach(function(b){"
        "b.addEventListener('click',function(){activeLayer=Number(b.dataset.layer);refreshLayerBtns();});"
        "});"
        "refreshLayerBtns();";
    if (!portrait) {
        html +=
        "document.getElementById('cimport').addEventListener('change',function(e){"
        "var f=e.target.files[0];if(!f)return;"
        "var status=document.getElementById('cstatus');"
        "function place(imgLike,iw,ih){"
        "if(!iw||!ih){status.textContent='Could not read that file.';return;}"
        "snap();"
        "var cvw=acv().width,cvh=acv().height;"
        "var scale=Math.min(cvw/iw,cvh/ih);"
        "var dw=iw*scale,dh=ih*scale,dx=(cvw-dw)/2,dy=(cvh-dh)/2;"
        "actx().drawImage(imgLike,dx,dy,dw,dh);"
        "captureFrame(false);status.textContent='';"
        "}"
        "if(f.type.indexOf('video')===0){"
        "status.textContent='Reading video frame\\u2026';"
        "var url=URL.createObjectURL(f);"
        "var vid=document.createElement('video');"
        "vid.muted=true;vid.playsInline=true;vid.src=url;"
        "vid.addEventListener('loadeddata',function(){"
        "try{vid.currentTime=Math.min(.1,vid.duration||0);}catch(err){}"
        "});"
        "vid.addEventListener('seeked',function(){"
        "place(vid,vid.videoWidth,vid.videoHeight);URL.revokeObjectURL(url);"
        "});"
        "vid.addEventListener('error',function(){"
        "status.textContent='Could not read that video.';URL.revokeObjectURL(url);"
        "});"
        "}else{"
        "status.textContent='Loading image\\u2026';"
        "var reader=new FileReader();"
        "reader.onload=function(ev){"
        "var img=new Image();"
        "img.onload=function(){place(img,img.naturalWidth,img.naturalHeight);};"
        "img.onerror=function(){status.textContent='That file is not a usable image.';};"
        "img.src=ev.target.result;"
        "};"
        "reader.readAsDataURL(f);"
        "}"
        "e.target.value='';"
        "});";
    }
    html +=
        "function flatten(){"
        "var out=document.createElement('canvas');"
        "out.width=layers[0].cv.width;out.height=layers[0].cv.height;"
        "var octx2=out.getContext('2d');"
        "octx2.fillStyle='#fff';octx2.fillRect(0,0,out.width,out.height);"
        "for(var i=0;i<layers.length;i++){"
        "octx2.globalAlpha=layers[i].opacity;octx2.drawImage(layers[i].cv,0,0);"
        "}"
        "octx2.globalAlpha=1;"
        "return out;"
        "}"
        "document.getElementById('cundo').addEventListener('click',function(){"
        "var L=layers[activeLayer];if(!L.undo.length)return;"
        "L.redo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "L.ctx.putImageData(L.undo.pop(),0,0);"
        "});"
        "document.getElementById('credo').addEventListener('click',function(){"
        "var L=layers[activeLayer];if(!L.redo.length)return;"
        "L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "L.ctx.putImageData(L.redo.pop(),0,0);"
        "});"
        "document.getElementById('cclear').addEventListener('click',function(){"
        "snap();var L=layers[activeLayer];L.ctx.clearRect(0,0,L.cv.width,L.cv.height);"
        "});";
    if (portrait) {
        // No frame history for a self-portrait -- captureFrame is called
        // from the shared pointer/import handlers above regardless of
        // mode, so it stays a real function here too, just a no-op; Save
        // does the only upload, straight to the existing /me/avatar route.
        html +=
            "function captureFrame(force,done){if(done)done();}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Saving\\u2026';"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();fd.append('photo',blob,'portrait.png');"
            "fetch('/me/avatar',{method:'POST',body:fd}).then(function(r){"
            "window.location=r.url||'/me';"
            "}).catch(function(){status.textContent='Could not save. Try again.';});"
            "},'image/png');"
            "});";
    } else if (post_mode) {
        // No frame history for a post either -- one flattened image, paired
        // with the caption, POSTed as one multipart request straight to
        // whichever composer route this page was opened for. r.url is the
        // real final URL fetch landed on after following that route's own
        // redirect, so a media error still surfaces as that route's own
        // ?err= message -- same pattern as the self-portrait Save above.
        html +=
            "function captureFrame(force,done){if(done)done();}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Posting\\u2026';"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();"
            "fd.append('text',document.getElementById('cpcaption').value);"
            "fd.append('media',blob,'post.png');"
            "fetch("+js_string_literal(post_target)+",{method:'POST',body:fd}).then(function(r){"
            "window.location=r.url||"+js_string_literal(post_target)+";"
            "}).catch(function(){status.textContent='Could not post. Try again.';});"
            "},'image/png');"
            "});";
    } else {
        html +=
            "var clearedOnce=false,frameCount="+std::to_string(existing_frames)+";"
            "function updateCounter(){"
            "document.getElementById('cframecount').textContent=frameCount+'/24 frames captured';"
            "}"
            // done() always fires, whether this capture actually uploaded
            // or was skipped (cap hit, no force) -- callers that need to
            // know the frame really landed (Save, below) depend on that.
            "function captureFrame(force,done){"
            "if(frameCount>=23&&!force){if(done)done();return;}"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();fd.append('frame',blob,'frame.png');"
            "function go(){"
            "fetch('/me/canvas/frame',{method:'POST',body:fd}).then(function(r){"
            "if(r.ok){frameCount++;updateCounter();}"
            "if(done)done();"
            "}).catch(function(){if(done)done();});"
            "}"
            "if(!clearedOnce){"
            "clearedOnce=true;"
            "fetch('/me/canvas/clear',{method:'POST'}).then(go).catch(go);"
            "}else{go();}"
            "},'image/png');"
            "}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Saving\\u2026';"
            "var timelapse=document.getElementById('ctimelapse').checked?'1':'0';"
            "captureFrame(true,function(){"
            "fetch('/me/canvas/mode',{method:'POST',"
            "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'timelapse='+timelapse}).then(function(){"
            "window.location='/u/'+"+js_string_literal(username)+";"
            "});"
            "});"
            "});";
    }
    html += "})();</script>";
    return html;
}

}  // namespace liquidgov::site
