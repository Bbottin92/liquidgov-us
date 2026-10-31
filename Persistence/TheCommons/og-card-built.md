# Share card — built, not deployed

Brandon asked for a unique sticker when a link is shared on Facebook or X,
with five stars and:

> It's like Myspace had a three-way with a ballot box and a gameboy
> — Anonymous

(The extra "with" in the ask was dropped.)

What ships

* New card `assets/og-image-v2.jpg` (1200×630): five brass stars, the
  quote, Anonymous, LiquidGov.US. New path so crawlers do not keep the
  old immutable `/assets/og-image.png`.
* Civic `<head>`: `og:description` / `twitter:description` is that quote.
  `og:image` points at v2.
* Genesis page quote updated to match (stars were already there).
* The Commons page carries the same tags, so a shared `/commons/` unfurls
  the same card.

Deploy is civic first (binary + `assets/og-image-v2.jpg` must be on the
box), then the client. After civic is up:

`curl -sI https://liquidgov.us/assets/og-image-v2.jpg` → 200, image/jpeg.
Paste `https://liquidgov.us/` into an X compose box and you should see
the stars and the quote. Facebook's debugger may need a scrape refresh.
