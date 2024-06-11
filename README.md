# Overview
Production ready **Fog of War** mechanic for RTS games in Unreal Engine 5. Check `Content/` folder for usage examples (`Test_FogOfWar` map). Core logic is implemented in C++ with performance in mind. The implementation is grid based with DDA raycasting. Tested in a real game with hundreds of units.

# Demo
<img src="demo.gif?raw=true">

# Interface
## Classes:

- **FogOfWar**: Contains the logic for the fog of war visibility. Must be spawned in the world (can be done dynamically) with a specified **GridVolume**.

  Properties (not all!):
  - **HeightScanCollisionChannel**: The collision channel to perform the heightscan on.
  - **GridVolume**: The volume on which the fog of war operates.
  - **TileSize**: The size of a tile in the grid. Smaller tiles result in higher grid resolution but slower performance.

  Debug Properties (not all!):
  - **bDebugStressTestIgnoreCache**: Update regardless of whether the actor's tile has changed.
  - **bDebugSnapshotTextureFilterNearest**: Apply a pixel filter to the visibility texture.

- **VisionComponent**: An ActorComponent attached to units that have a visibility radius around them. The **SightRadius** can be set (this property can also be adjusted via a slider in the editor at runtime). The radius can be changed at runtime, but it is recommended to do this sparingly as it is performance-intensive.

- **VisibleComponent**: An ActorComponent attached to actors to automatically update whether the actor is visible or not. By default, if the actor is not visible, it is hidden (this logic can be disabled by setting the **bManageOwnerVisibility** property to false). It is also possible to subscribe to **OnVisibilityChanged** – this event is triggered when the visibility of the actor changes (useful for implementing additional logic).

**Note!!!** The GameState must have a **UManagerComponent** (just add it, no properties there).

# High-Level Implementation
**FogOfWar** after initialization registers with the manager. **VisionComponent** and **VisibleComponent** wait for **FogOfWar** to register with the manager before they initialize, and **VisionComponent** also registers with **FogOfWar**. Registering a **VisionComponent** allocates a local visibility area around the actor. When the **VisionComponent** is destroyed, it unregisters from **FogOfWar** (this local area is deleted with some logic). If **SightRadius** is changed at runtime, a re-registration occurs (deallocating the old local area and allocating a new one). When **VisionComponent** changes the tile it is on, the local visibility area is recalculated, and changes are applied to the global visibility area (the global area is the entire grid). This approach enhances performance for static actors and actors that rarely or slowly move. The height map is calculated during initialization for each tile using the **HeightScanCollisionChannel** channel (**Camera** by default).

# Stat
`stat FogOfWar`
