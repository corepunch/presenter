layout: title
notes: Welcome everyone. Today I'll walk you through our SDL2 Game Engine project — a modern rendering pipeline built from the ground up. This project started about 14 months ago with the goal of creating a performant 2D/3D engine using SDL2 as the foundation.

# **SDL2** Game Engine

A Modern Rendering Pipeline

layout: content
notes: Here are the four pillars of our engine. Rendering uses a deferred pipeline with OpenGL 4.5 core. Audio is handled via SDL_mixer with spatial positioning. Input abstraction supports keyboard, mouse, and gamepad simultaneously. Physics uses a custom lightweight system optimized for 2D tile-based worlds.

# Key Features

- **Deferred** rendering pipeline with OpenGL 4.5 and custom shader system
- **Spatial** audio engine with real-time mixing and 3D positional audio
- **Unified** input layer abstracting keyboard, mouse, and gamepad devices
- Lightweight physics system with AABB collision and spatial hashing

layout: section
notes: Let's dive into the architecture. I'll explain how the core systems connect and what design patterns we used to keep the codebase maintainable as it grew from 5K to 45K lines.

# Architecture

layout: image
notes: This diagram shows our rendering pipeline. Geometry is submitted through a scene graph, transformed into G-buffer passes, then resolved in the lighting stage. We use two fullscreen quads for the ambient and deferred passes. The final composite stage applies post-processing effects like bloom and tonemapping.

# Rendering Pipeline

![pipeline](./assets/pipeline.png)

The deferred rendering path from geometry submission to final framebuffer output.

layout: two-column
notes: The old renderer was a forward renderer with hardcoded shaders. It couldn't handle more than 200 sprites at 60fps. The new deferred system batches draw calls, uses instanced rendering for sprites, and supports dynamic lighting. We went from 200 to 10,000 sprites while maintaining 60fps on integrated graphics.

# Comparison

---left---

### Old Renderer

- Forward rendering with single-pass
- Hardcoded shader programs
- ~200 sprites at 60fps
- No dynamic lighting support
- Manual texture atlas management

---right---

### New Renderer

- Deferred rendering with G-buffer
- Hot-reloadable shader system
- ~10,000 sprites at 60fps
- 64 dynamic lights per frame
- Automatic atlas packing and streaming

layout: content
notes: Let me walk through the benchmarks. We ran these on a mid-range laptop — i7-12700H with integrated Iris Xe. Draw calls dropped from 1,200 to 89 thanks to instancing. Frame time went from 16.7ms to 2.1ms. Memory usage actually decreased because we eliminated redundant texture copies. The profiling tools we built into the engine made this optimization cycle much faster.

# Performance Results

- Draw calls reduced from 1,200 to 89 (93% reduction) via GPU instancing
- Frame time improved from 16.7ms to 2.1ms on integrated graphics
- Memory usage decreased by 34% through shared texture atlases
- Shader compilation time reduced to <50ms with parallel compilation

layout: title
notes: That concludes the overview. Our engine has come a long way from a simple sprite renderer to a full-featured deferred pipeline. Next steps include adding skeletal animation, a particle system, and eventually Vulkan support. I'm happy to take any questions about the architecture, performance, or design decisions.

# Thank You

Questions?
