# The MAINstream map as waterways, and peeking through portals (questions before building)

Asked by Claude, 2026-11-14, after Brandon's direction (the dated bullet "Next direction for walking, the map and
pages" in `Persistence/notes.md`). Slice A of that direction (always centered, a menu on your drop with microphone,
camera and drip speed) is built; nothing below is.

Answer with the letters (for example "1B 2A 3A 4B"), or in your own words.

## What exists today

- The map is a graph of boxes joined by straight connector lines. Opening a box puts it at the center with its
  linked boxes in a ring around it; the trail at the top records the path you took.
- The top level ("you" box) is ringed by: Politics, Friends, Events, Feeds, Places, Groups, Interests
  (`map_graph.cpp` `map_mine_links`). Each one lists real things: your friends, your groups, your cities, proposals
  you touched, feeds with posts in them, your interests.
- Your drop walks freely anywhere within 150 map units of the boxes. Being inside a box means you are in it (its
  instance): presence, dots and the camera bubble all work per box.
- A box's page opens in a side panel on the right of the map.

## 1. The water

"you should be bound to the lakes and streams and pools like youre in a closed loop system."

- **1A (Claude's recommendation).** Every box is a pool or lake (a large rounded body of water). Every connector is
  a stream (a channel a few drops wide). You can only be in water: walking into the bank stops you, the way a closed
  valve does now. Streams join only boxes that are linked, so the map's links are literally the routes.
- **1B.** The same, plus the streams flow: drifting along a stream with the current is faster, against it slower
  (like the moving sidewalk on the home page).
- **1C.** Something else (say what).

## 2. How big a place is

"significantly larger, as if theyre like physical places you can be and hang out at with friends."

- **2A (recommended).** A place is roughly a phone screen across at the walking zoom, so you can stand in it with
  several friends and see each other's dots and camera bubbles without the place filling the whole map. The walking
  zoom shows the place you are in plus the mouths of the streams leaving it.
- **2B.** Each place fills the screen; you only see the next place once you swim down a stream toward it.

## 3. The top level

"the top level perspective of the mainstream media map would be like people, places, things, and ideas"

Claude's proposed sorting of what exists today:

| Top level | What it holds now |
|---|---|
| People | your friends; then people near you (your cities) |
| Places | your cities; your groups (a group is a place you meet) |
| Things | feeds and posts; events (none exist yet) |
| Ideas | your interests; politics (proposals you touched); problems you noted |

- **3A (recommended).** That table. The "you" pool is in the middle; four streams lead to four lakes named People,
  Places, Things and Ideas.
- **3B.** The same four, but groups count as People (a group is its members), not Places.
- **3C.** A different sorting (say how).

## 4. "Roads of relatively connected concepts or objects"

"some inteligent and dynamic way for things to populate as you walk around"

- **4A (recommended).** As you swim down a stream, the places ahead are filled in from what links to where you are,
  using links the site already knows: shared groups, shared interests, the same city, proposals you both voted on,
  who posted in which feed. Ranked so the closest relations come first. No AI service: it works from the site's own
  records, so it is explainable ("you are both in Fresno Neighbors").
- **4B.** The same, plus an AI model suggests related concepts from the text of posts and proposals. More surprising,
  but it costs money per use, sends text to an outside service, and is harder to explain. Would need its own
  decision about what text may leave the server.
- **4C.** Start with 4A; decide on 4B later.

## 5. Peeking through a portal, and the call carrying over

"walk up to the portal ... a small semi-translucent window blow up from the center of the portal when you get within
entry range. if you click A within that range, youd see that window expand to fill the screen while simultaneously
becoming opaque until it effectively becomes the page youre occupying" and "the microphone and camera should be
consistent and seamless when transitioning from the mainstream map to a page."

How it would work (one mechanism for both): the page you walk into is loaded in place, without the browser leaving
the map, so the microphone, camera and anyone you are talking to stay connected. Today every page is a full reload,
which drops the call.

- **5A (recommended).** The peek shows the real page, live, shrunk into the window. A grows it to full screen and
  you are on that page (its address shows in the browser bar, Back returns you to the map where you stood). Your call
  carries over. The side panel goes.
- **5B.** The peek shows only a preview card (title, picture, who is there); the page itself loads after A. Simpler
  and lighter on phones, but the peek is not the page.

Two consequences to know for 5A: pages loaded in place must not run their scripts twice (some pages, like Juketube,
will need care), and the microphone and camera then also work on the map itself, not only on profile pages.

## Order Claude suggests

1. Slice B: portal peek and the call carrying over (5), since it replaces the side panel and is mostly page code.
2. Slice C: the water and the bigger places (1, 2), then the four lakes (3), then the populating streams (4).
