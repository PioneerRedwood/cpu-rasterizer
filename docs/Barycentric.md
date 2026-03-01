# Barycentric Triangle Rasterization Notes

This document explains triangle filling with barycentric coordinates, with examples aligned to this repository.

## 1) Coordinate conventions in this codebase

- Vector/matrix convention: row-vector (`v * M`).
- 3D convention: left-handed (LH), forward is `+Z`.
- Screen convention after viewport transform: origin at top-left, `+Y` goes down.

Important implication:
- World/view/projection orientation and screen-space orientation are not the same.
- Triangle winding may appear flipped after transforms if you compare different spaces.

## 2) Core idea of barycentric fill

Any point `P` in triangle `(A, B, C)` can be represented as:

```txt
P = w0 * A + w1 * B + w2 * C
w0 + w1 + w2 = 1
```

If `P` is inside the triangle, all weights are non-negative (for a consistent winding test).

In rasterization we do not usually compute weights with matrix inversion per pixel.
Instead we use edge functions because they are cheap and robust.

## 3) Edge function

Given directed edge `A -> B`, edge function at point `P(x,y)`:

```txt
E(A,B,P) = (Px - Ax) * (By - Ay) - (Py - Ay) * (Bx - Ax)
```

Interpretation:
- `E > 0`: `P` is on one side of the edge.
- `E < 0`: `P` is on the opposite side.
- `E = 0`: `P` is on the edge.

For triangle fill, evaluate:

- `w0 = E(B, C, P)`
- `w1 = E(C, A, P)`
- `w2 = E(A, B, P)`

## 4) Why both CW and CCW checks are needed

The sign of all edge values flips when vertex order flips:

- CCW triangle: inside is usually `w0 >= 0 && w1 >= 0 && w2 >= 0`
- CW triangle: inside is usually `w0 <= 0 && w1 <= 0 && w2 <= 0`

So if winding is not enforced, you must accept both sign patterns.

Alternative:
- Compute signed area once (`area = E(A,B,C)`).
- Multiply all edge values by `sign(area)`.
- Then only test `>= 0`.

## 5) Practical rasterization pipeline

1. Transform vertices to screen space.
2. Compute triangle AABB (`minX/maxX/minY/maxY`).
3. Clamp AABB to framebuffer bounds.
4. Iterate pixels in AABB.
5. Sample at pixel center: `P = (x + 0.5, y + 0.5)`.
6. Evaluate edge functions and inside test.
7. If inside, write color/depth.

Why pixel center sampling:
- Reduces bias and gives more stable edge behavior than corner sampling.

## 6) Degenerate triangles

If `area` is near zero, triangle is line-like or point-like.
Skip fill to avoid unstable results.

Typical guard:

```txt
if (abs(area) < epsilon) return;
```

## 7) Attribute interpolation with barycentric weights

Once `w0,w1,w2` are known, any per-vertex attribute can be interpolated:

```txt
attr(P) = w0 * attr0 + w1 * attr1 + w2 * attr2
```

Examples:
- color
- UV
- normal
- depth

For perspective projection, use perspective-correct interpolation for attributes like UV.

## 8) Common engineering pitfalls

- Not handling winding consistently.
- Forgetting to clamp AABB to screen bounds.
- Sampling pixel corner instead of center.
- No top-left rule, causing cracks or overlaps between adjacent triangles.
- No depth buffer, causing wrong visibility.
- Recomputing full edge math each pixel (slow).

## 9) Performance notes

Baseline implementation evaluates edge equations per pixel directly.
Faster approach uses incremental stepping:

- Move `x` by +1: each edge value changes by a constant delta.
- Move `y` by +1: each edge value changes by another constant delta.

This reduces per-pixel ALU and is the standard software rasterizer optimization.

## 10) Minimal pseudocode

```txt
area = E(v0, v1, v2)
if abs(area) < eps: return

for y in [yMin..yMax]:
  for x in [xMin..xMax]:
    p = (x + 0.5, y + 0.5)
    w0 = E(v1, v2, p)
    w1 = E(v2, v0, p)
    w2 = E(v0, v1, p)

    insideCCW = (w0 >= 0 && w1 >= 0 && w2 >= 0)
    insideCW  = (w0 <= 0 && w1 <= 0 && w2 <= 0)
    if (area > 0 && insideCCW) || (area < 0 && insideCW):
      drawPixel(x, y)
```

This pseudocode matches the current `fillTriangleBarycentric` implementation.

## 11) How modern GPUs and drivers do this (high level)

A CPU barycentric rasterizer is conceptually correct, but production GPUs execute a more specialized pipeline.

### A) Front-end and primitive setup

1. Vertex shading:
- Transforms vertices to clip space.
- Produces attributes (UV, color, normal, etc.).

2. Primitive assembly and clipping:
- Builds triangles from vertex indices.
- Clips against frustum/guard-band.

3. Triangle setup (fixed-function hardware):
- Converts triangle edges into edge equations.
- Computes interpolation planes/gradients once per triangle.
- Uses fixed-point/integer math heavily for speed and determinism.

This setup stage is the hardware equivalent of precomputing values that a naive CPU loop would recompute per pixel.

### B) Rasterization details

- Coverage test still uses edge-equation style logic.
- Rasterization usually runs in small quanta (commonly 2x2 pixel quads), not one scalar pixel at a time.
- Top-left fill rule is enforced in hardware to avoid cracks/double-hit on shared edges.
- MSAA evaluates per-sample coverage (not just pixel center).

So barycentric/edge logic is still there, but vectorized and deeply pipelined.

### C) Interpolation and fragment stage

- Hardware computes perspective-correct interpolation using precomputed gradients.
- Fragment/pixel shaders run for covered samples/fragments.
- Derivatives (`ddx/ddy`) are naturally available because shading happens in quads.

This is why texture LOD and normal mapping are efficient on GPUs.

### D) Visibility and bandwidth optimizations

Modern GPUs avoid unnecessary fragment work aggressively:

- Back-face culling before raster.
- Early-Z / Hierarchical-Z to reject occluded fragments before running fragment shader.
- Depth/stencil compression and fast clears.
- Color/depth cache hierarchies to reduce memory traffic.

In many scenes, these optimizations dominate performance gains more than raw ALU speed.

### E) Tiled rendering approaches

There are two broad architectures:

- Immediate-mode renderers (common on desktop): process primitives roughly in submission order with strong cache/compression optimizations.
- Tile-based deferred renderers (common on mobile): bin triangles into tiles, rasterize tile-local data on-chip, then flush, minimizing external memory bandwidth.

Both still rely on edge equations and barycentric interpolation, but scheduling/memory behavior differs.

### F) Driver responsibilities

The graphics driver typically:

- Validates state and API commands.
- Compiles/optimizes shaders (often asynchronously, with caching).
- Builds command buffers for GPU front-end.
- Reorders/batches work where legal.
- Manages synchronization, residency, and pipeline hazards.

The driver generally does not replace triangle rasterization math with a different algorithm; it programs hardware units and optimizes how work reaches them.

## 12) Practical roadmap for this renderer

The current CPU barycentric rasterizer is a strong educational baseline.
To move closer to GPU-like behavior, useful incremental improvements are:

1. Incremental edge stepping (avoid recomputing full edge function each pixel).
2. Top-left rule for exact shared-edge behavior.
3. Z-buffer and early depth test in the raster loop.
4. Perspective-correct attribute interpolation.
5. Tile/bin based raster traversal for cache locality.
