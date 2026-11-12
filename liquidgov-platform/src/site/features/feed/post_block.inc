// viewer is the logged-in username rendering this post (empty for an
// anonymous visitor to a public profile) -- only a logged-in viewer gets a
// Repost button. Reposting always credits the true original author (see
// the repost_of_author comment on Post), so a chain of reposts never grows
// deeper than one hop of attribution.
std::string post_block(const Post& p, const std::string& viewer) {
    std::string where;
    if (!p.community.empty())
        where = " in <a href=\"/c/"+html_escape(p.community)+"\">"+html_escape(p.community)+"</a>";
    std::string when = fmt_time(p.ts);
    std::string repostLine;
    if (!p.repost_of_author.empty())
        repostLine = "<p class=\"muted\">&#8635; Reposted from <a href=\"/u/"+html_escape(p.repost_of_author)+
                     "\">"+html_escape(p.repost_of_author)+"</a></p>";
    std::string repostForm;
    if (!viewer.empty())
        repostForm = "<form class=\"inline\" method=\"POST\" action=\"/me/repost\">"
                     "<input type=\"hidden\" name=\"post_id\" value=\""+std::to_string(p.id)+"\">"
                     "<button class=\"ghost\" type=\"submit\">&#8635; Repost</button></form>";
    const int seconds = g_store.second_count(p.id);
    std::string secondForm;
    if (!viewer.empty()) {
        const bool seconded = g_store.has_seconded(p.id, viewer);
        secondForm = "<form class=\"inline\" method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/second\">"
                     "<button class=\""+std::string(seconded?"":"ghost")+"\" type=\"submit\">"+
                     std::to_string(seconds)+(seconds==1?" second":" seconded")+
                     (seconded?" &#10003;":"")+"</button></form>";
    } else if (seconds > 0) {
        secondForm = "<p class=\"muted\">"+std::to_string(seconds)+(seconds==1?" second":" seconded")+"</p>";
    }
    std::string deleteForm;
    if (viewer == p.author)
        deleteForm = "<form class=\"inline\" method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/delete\" "
                     "onsubmit=\"return confirm('Delete this post?');\">"
                     "<button class=\"ghost\" type=\"submit\">Delete</button></form>";
    std::string actionsRow;
    if (!repostForm.empty() || !secondForm.empty() || !deleteForm.empty())
        actionsRow = "<div class=\"row\" style=\"margin-top:.3rem\">"+repostForm+secondForm+deleteForm+"</div>";
    std::string repliesBlock;
    {
        auto replies = g_store.replies_of(p.id);
        std::string list;
        for (const auto& r : replies) {
            std::string rwhen = fmt_time(r.ts);
            list += "<div class=\"who\" style=\"margin:.5rem 0\">"+portrait_tag(r.author,"sm")+
                    "<div><a href=\"/u/"+html_escape(r.author)+"\"><strong>"+html_escape(r.author)+
                    "</strong></a> <span class=\"muted\">"+html_escape(rwhen)+"</span>"
                    "<p style=\"margin:.2rem 0 0\">"+html_escape(r.text)+"</p></div></div>";
        }
        std::string replyForm;
        if (!viewer.empty()) {
            replyForm = "<form method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/reply\" style=\"margin-top:.3rem\">"
                        "<textarea name=\"text\" maxlength=\"1000\" placeholder=\"Write a reply\"></textarea>"
                        "<button class=\"ghost\" type=\"submit\">Reply</button></form>";
        }
        if (!replies.empty() || !viewer.empty()) {
            repliesBlock = "<details style=\"margin-top:.4rem\"><summary class=\"muted\" style=\"cursor:pointer\">"+
                std::to_string(replies.size())+(replies.size()==1?" reply":" replies")+"</summary>"
                "<div style=\"padding-left:.8rem;border-left:2px solid var(--rule);margin-top:.3rem\">"+
                list+replyForm+"</div></details>";
        }
    }
    std::string mediaHtml;
    if (!p.attachment_blob_id.empty()) {
        std::string src = "/post-media/"+html_escape(p.attachment_blob_id);
        // width:100% (not max-width) so a small source image still fills
        // the card instead of rendering at its native pixel size -- the
        // bug that first surfaced this: a 120x80 test PNG showed up
        // genuinely postage-stamp-sized, since max-width alone only ever
        // scales large images down, never scales small ones up.
        mediaHtml = (p.attachment_mime.rfind("video/",0)==0)
            ? "<video controls preload=\"metadata\" style=\"width:100%;height:auto;display:block;border-radius:var(--rsm);margin-top:.4rem\" src=\""+src+"\"></video>"
            : "<img style=\"width:100%;height:auto;display:block;border-radius:var(--rsm);margin-top:.4rem\" src=\""+src+"\" alt=\"\">";
    }
    return "<article class=\"post\" id=\"post-"+std::to_string(p.id)+"\"><div class=\"who\">"+portrait_tag(p.author,"sm")+"<div><a href=\"/u/"+html_escape(p.author)+"\"><strong>"+
           html_escape(p.author)+"</strong></a>"+where+
           (when.empty()?"":" <span class=\"muted\">"+html_escape(when)+"</span>")+
           repostLine+
           "<p>"+linkify_post_text(p.text)+"</p>"+mediaHtml+actionsRow+repliesBlock+"</div></div></article>";
}

