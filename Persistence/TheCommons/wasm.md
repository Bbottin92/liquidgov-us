# WebAssembly — game logic

Not a BUILD. Does not replace sprites, glue, or sibling. `commons/` stays
one vanilla HTML file until Brandon picks a next slice.

## The honest scale

Collision is a 12×12 box vs a `Uint8Array`. Movement is `speed * dt` on
two axes. Presence is ~2 Hz. The blit is one `drawImage` of 150 tiles.
None of that is a JS bottleneck. WASM will not make the Square feel
better. Sprites will.

## When WASM would actually earn a slot

**One collision/movement core, two hosts.** If the droplet daemon is C++
(civic already is) and the browser must not trust the client’s “I walked
through the boarded door,” compile that core to:

- native `.o` linked into the daemon (authoritative)
- `.wasm` loaded by `commons/` (prediction / feel)

Same `solid[]`, same `DT_MAX`, same enter/exit rules. Cheating a door
fails server-side even if the client WASM is patched.

That is a **sibling-era** split, not a V1 rewrite. Until there is a
daemon, there is no second host to share with.

## What WASM must not own

| Keep in JS / canvas | Why |
|---|---|
| Bake + `drawImage` + CSS scale | Already the cheap path (`tilemap.md`) |
| D-pad / keyboard / dual crop | DOM + `art.md` |
| Toasts, Hall `confirm`, `window.open` | Browser APIs |
| Throwaway name overlay | HTML |
| Presence subscribe / WebSocket | I/O, not compute |

Do not Emscripten the whole page. Do not ship a 2 MB `game.js` glue
file to move a 12 px box.

## Cost of doing it now

- Build step (clang/wasm-pack). Breaks “one HTML file, no npm.”
- Debugging collision in DWARF vs the current 40-line `movePlayer`.
- Still need JS for input and blit, so two languages for one preview.

`sessionStorage` and IndexedDB were fake persistence. WASM-now would be
fake architecture: a second toolchain with no second process.

## Later shape (if sibling is C++)

```
collision.cpp  →  libcommons.a     (daemon)
               →  collision.wasm   (client, optional prediction)
JS: input, rAF, canvas, cookie, WS
```

Export a tiny C ABI: `blocked(cx,cy,half)`, `move(id,dx,dy,dt)`,
`region_at(x,y)`. No STL in the WASM boundary. No canvas in C++.

If the daemon is not C++, skip WASM and keep JS collision; the server
re-simulates in whatever language it is. Dual implementation of a
12-line AABB is cheaper than a compile farm.

## Reviewer fail

- Adding a WASM toolchain to P0–P3 `commons/index.html`
- WASM for “performance” of fillRect sprites
- Putting presence or SQLite inside the module
- Emscripten SDL canvas “port” of the whole game
