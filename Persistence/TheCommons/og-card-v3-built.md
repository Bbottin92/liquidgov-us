# Share card, the hook — built, not deployed

Brandon: the preview that shows up when someone pastes liquidgov.us into
Facebook or X needs a civic hook, not the joke that is on the card now.

The joke was his own request when the card was made (`og-card-built.md`).
The landing page has since moved. The hero is "This is YOUR government!"
(`58c1aa5`). The card under it says this is a space to organize and build
the systems a real government will need, and that it does not replace the
city, state, or federal government you already live under (`3be1692`).
The share card was the piece that still said Myspace.

## What the card says

The image is the hook. The description is the limit, because a share that
only shouted "this is your government" would claim the authority the page
itself refuses.

* `assets/og-image-v3.jpg`, 1200×630. Aged paper, a brass rule, the
  wordmark, "A Voluntary Natural Democracy." No stars. The five-star
  review frame was the joke's vehicle, and a rating of a government is
  the wrong picture for this line.
* The line on the card, from the hero and the first clause of the lede:
  **This is YOUR government.** You represent yourself.
* `og:description` and `twitter:description`, one string for the whole
  site, including The Commons: "This is YOUR government. You represent
  yourself — a voluntary association, not a replacement for the one you
  already live under."
* New path. v2 is cached immutable for a year. Reusing it would leave
  some shares on the joke forever. The v2 route still serves the old
  file, so a crawler that already stored that URL does not 404.

The anonymous quote on the genesis page is unchanged. That is a card on
the page, not the unfurl.

## What has to be on the box

Civic first: the binary (it is what answers `/assets/og-image-v3.jpg`)
and `assets/og-image-v3.jpg` beside it. Then the Commons client, or a
shared `/commons/` link still unfurls the joke from the old static tags.

Not deployed. The live URL was checked and does not have the file yet:

`curl -sI https://liquidgov.us/assets/og-image-v3.jpg` is not 200 until
that civic deploy. After it is:

the same curl should be `200` and `image/jpeg`. Paste
`https://liquidgov.us/` into an X compose box. The card should say
"This is YOUR government." Facebook may need a scrape refresh.
