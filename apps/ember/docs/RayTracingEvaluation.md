# Ray Tracing Evaluation

This document surveys options for adding real‑time ray tracing to Ember's
OGRE based renderer and outlines a possible integration strategy.

## OGRE 2.x

OGRE 2.x currently focuses on rasterization and does not ship with a
complete ray‑tracing pipeline.  The engine provides a modern rendering
architecture and an abstraction for low‑level APIs (OpenGL, Vulkan),
but lacks acceleration‑structure management or ray‑tracing stages.  To
use ray tracing with OGRE 2.x we must access the underlying graphics API
and manage TLAS/BLAS resources ourselves.

## External APIs

### Vulkan Ray Tracing
* KHR_ray_tracing extensions are cross‑vendor and integrate naturally
  with OGRE's Vulkan backend.
* Requires explicit creation of bottom‑level (BLAS) and top‑level (TLAS)
  acceleration structures and shader pipelines.
* Suitable for Linux and Windows builds.

### DirectX Raytracing (DXR)
* Windows only, implemented through D3D12.
* Similar concepts to Vulkan: BLAS/TLAS and dedicated shader stages.
* OGRE's D3D12 backend could be extended to expose DXR objects when
  running on supported hardware.

## Hybrid Rendering Approach

1. **Raster base pass** – continue to use OGRE's existing render
   pipeline for the main geometry pass.
2. **Ray‑traced effects** – build acceleration structures from dynamic
   scene geometry and dispatch ray‑tracing shaders for shadows,
   reflections and global illumination.
3. **Composition** – combine ray‑traced results with the raster output
   via screen‑space compositors.

## Configuration

Ray‑tracing features should be optional.  Configuration keys proposed:

```ini
[rendering]
rt_shadows = false
rt_reflections = false
rt_globalillumination = false
```

These are wired into the `Application` class through `RayTracingState`
which exposes runtime toggles for the effects.

## Next Steps

* Implement TLAS/BLAS builders for world geometry under
  `apps/ember/src/`.
* Add Vulkan/DXR specific shader pipelines.
* Extend scene update logic to rebuild acceleration structures for
  dynamic objects.
* Use the configuration switches to enable or disable ray‑traced passes
  at runtime.
