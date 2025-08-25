# Asset Authoring Workflow

This document outlines the updated workflow for creating assets with physically based rendering (PBR) textures.

## Required Maps

Ember now supports the following texture maps for materials:

- **Albedo Map**: base color without lighting.
- **Metallic Map**: metallic values stored in the red channel.
- **Roughness Map**: surface roughness stored in the red channel.
- **Normal Map**: tangent-space normals encoded in RGB.
- **Ambient Occlusion (AO) Map**: ambient occlusion in the red channel.

## Preparing Materials

1. Place the texture files in your asset directory.
2. Reference them from a material that inherits from `/common/base/pbr`.
3. Assign the textures to the appropriate `AlbedoMap`, `MetallicMap`, `RoughnessMap`, `NormalMap`, and `AOMap` texture units.

## Generating Mipmaps

Large textures (such as 4K) will automatically generate mipmaps during background loading. This improves runtime performance and memory usage.

## Streaming

Textures are streamed in the background during model loading to keep the frame rate stable. Ensure your assets are registered with the `ModelBackgroundLoader` so streaming can begin as early as possible.
