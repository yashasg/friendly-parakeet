# Skill: Raylib Owned Model Tint

## Use when

- An entity is rendered via `ObstacleModel`/`Model` ownership path (`DrawMesh` over model meshes).
- Visual color should follow ECS `Color` but appears white or default.

## Pattern

When drawing owned models, query `Color` alongside the model and apply tint on a local material copy:

```cpp
Material mat = om.model.materials[om.model.meshMaterial[i]];
mat.maps[MATERIAL_MAP_DIFFUSE].color = tint;
DrawMesh(om.model.meshes[i], mat, om.model.transform);
```

## Why

Owned-model paths bypass `ModelTransform` helpers, so tinting is not automatic. Explicitly applying `Color` keeps visuals consistent with mesh-child/model-transform rendering and prevents unintended white geometry.

## Applied in this repo

- `app/systems/game_render_system.cpp` (`draw_owned_models`).
