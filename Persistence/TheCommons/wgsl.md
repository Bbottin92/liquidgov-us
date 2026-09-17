# WGSL compute crib

Syntax only. **Do not add a `.wgsl` file or `navigator.gpu` to
`commons/`.** See `webgpu.md` — this plaza does not have a GPU job.

## Smallest dispatch

```wgsl
@group(0) @binding(0) var<storage, read>        src: array<f32>;
@group(0) @binding(1) var<storage, read_write>  dst: array<f32>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
  let i = gid.x;
  if (i >= arrayLength(&src)) { return; }
  dst[i] = src[i] * 2.0;
}
```

JS side: `dispatchWorkgroups(ceil(n / 64))`. One dimension is enough
until you have a 2D grid.

## Pieces

| Thing | WGSL |
|---|---|
| Immutable | `let x = 1u;` |
| Mutable | `var x: f32 = 0.0;` |
| Module const | `const N: u32 = 64u;` |
| Types | `f32` `u32` `i32` `bool` `vec2/3/4<T>` `mat4x4<f32>` |
| Storage | `var<storage, read>` / `read_write` (SSBO) |
| Uniforms | `var<uniform> params: Params;` (16-byte aligned) |
| Workgroup shared | `var<workgroup> tile: array<f32, 64>;` then `workgroupBarrier()` |
| Atomics | `atomic<u32>`, `atomicAdd` — counters, not game state |
| Textures | `texture_storage_2d<rgba8unorm, write>` if you *must* write pixels |

Entry **must** be `@compute @workgroup_size(x, y, z)`. `@builtin` ids:
`global_invocation_id`, `local_invocation_id`, `workgroup_id`,
`local_invocation_index`.

No recursion. No `throw`. Pointers exist (`&src`) but you do not
malloc. `if (i >= n) { return; }` is the real bounds check — GPU will
not do it for you.

## Host (only so the crib is complete)

```
pipeline = device.createComputePipeline({ compute: { module, entryPoint: "main" } })
pass = encoder.beginComputePass()
pass.setPipeline(pipeline)
pass.setBindGroup(0, bindGroup)
pass.dispatchWorkgroups(groups)
pass.end()
```

Readback is a staging buffer + `mapAsync`. That round-trip is why
collision in a shader is slower than `solid[i]` in JS (`tilemap.md`).

## For this repo

If a future product actually has a compute job, start from the snippet
above. The Commons’ next visible work is still sprites / glue / sibling,
not a shader.
