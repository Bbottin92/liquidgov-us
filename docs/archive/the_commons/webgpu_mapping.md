# WebGPU buffer mapping

Crib only. Still **no** `navigator.gpu` in `commons/` (`webgpu.md`).
This is why a compute collision pass loses to `solid[i]`.

## The rule

A buffer is either **on the GPU** or **mapped to JS**. Never both.
`mapAsync` is a fence: the Promise resolves after the GPU is done with
it. `getMappedRange()` is a CPU `ArrayBuffer`. `unmap()` gives it back.

You also cannot set `STORAGE | MAP_READ` on the same buffer. Readback
is always a **copy into a staging buffer**.

## Readback (the slow path)

```
gpuBuf     = STORAGE | COPY_SRC          // compute writes here
staging    = MAP_READ | COPY_DST         // CPU reads here

encoder.copyBufferToBuffer(gpuBuf, 0, staging, 0, bytes)
encoder.finish() → queue.submit()
await staging.mapAsync(GPUMapMode.READ)
const view = new Float32Array(staging.getMappedRange())
// ...copy out; the range is invalid after unmap
staging.unmap()
```

`mapAsync` offset and size must be multiples of **8**. Map the whole
staging buffer if you don’t want to think about it.

Until `mapAsync` resolves you do not have the data. That is at least
one frame of latency, often more. Fine for a screenshot. Death for
per-step collision.

## Upload

```
staging = MAP_WRITE | COPY_SRC
gpuBuf  = STORAGE | COPY_DST

await staging.mapAsync(GPUMapMode.WRITE)
new Float32Array(staging.getMappedRange()).set(src)
staging.unmap()
encoder.copyBufferToBuffer(staging, 0, gpuBuf, 0, bytes)
```

Or `queue.writeBuffer(gpuBuf, 0, src)` — skips the staging map for
small uniform-sized uploads. Prefer that for a 256-byte param block.
Do not `writeBuffer` 5.6 MB of bake every frame.

## Invalid states (reviewer fail if we ever did this)

- `getMappedRange()` before `mapAsync` resolves
- Using the buffer in a compute pass while mapped
- Leaving it mapped across `submit` (unmap first)
- Reading `getMappedRange()` after `unmap` (detached)
- Mapping a `STORAGE` buffer directly

## For The Commons

Player step is CPU, 3.8 px, `Uint8Array`. Mapping a GPU buffer to ask
“is this tile solid?” is a round-trip to answer one bit. Don’t.

Next visible work is still sprites / glue / sibling.
