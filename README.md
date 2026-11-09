# İstanbul Taksi Simülatörü (UE 5.4 C++)

An atmospheric, 100% C++ procedural Istanbul taxi driving simulator built on **Unreal Engine 5.4** (Win64 Development Editor). Every urban corridor, architectural block, vehicle mesh, traffic chokepoint, Bosphorus water shader, and passenger interaction spawns and configures automatically from C++ with zero manual Blueprint or Content Browser setup required.

---

## Core Architecture & Classes

### 1. Urban Geography, Junction Solver & Procedural Architecture
- **`ATestSceneBuilder`** (`Source/IstanbulTaxi/Core/TestSceneBuilder.h/.cpp`):
  - Generates a multi-district, continuous road network (`TArray<FIstanbulRoadSegmentSpec>`) spanning **Eminönü Coastal Boulevard**, **Galata Bridge** (two-level deck with HISM angler fishing rods), **Sirkeci–Sultanahmet Tram Avenue**, **Sultanahmet Plaza** (Ottoman marble fountain & *Dikilitaş* / Obelisk of Theodosius), **Tarihi Yarımada Narrow Cobblestone Backstreets**, and **Galata / Cihangir Steep Hills** (`12.5°–17.6°` grades with a midpoint `DUR` stop-sign switchback).
  - **Junction Solver (`SolveAndBuildJunctions`)**: Unifies node elevations across all connected road segments and generates rounded `3–6 m` curb corner returns, intersection road pads, and zebra crosswalk strips (`FIstanbulMeshUtils::AppendJunctionPatch`).
  - **Automated Quality Gate (`ValidateNetwork()`)**: Verifies `0` node elevation mismatches, `WidthM >= 3.5 m` on all segments, `GradeDegrees <= 18.0°`, and `0` building/cumba intrusions into drivable road corridors.
- **`AIstanbulBuildingBlock`** (`Source/IstanbulTaxi/Environment/IstanbulBuildingBlock.h/.cpp`):
  - Data-driven via `FIstanbulDistrictSpec` (`WideAvenue`, `NarrowAlley`, `Plaza`, `Waterfront`, `HillResidential`) and `FIstanbulBuildingSpec`.
  - Batches multi-story (`2–8` floor) attached Istanbul buildings (*bitişik nizam*) into a single `UProceduralMeshComponent` per block with simple `UBoxComponent` collision, stone ground floors, shopfronts, awnings, pilasters, cornices, distance-culled HISM AC units & rooftop water tanks, corbelled **Cumba** bay windows (`0.6–1.0 m` overhang clamped to sidewalk clearance), a spanning arched alley gateway (`3.9 m >= 3.2 m` vertical clearance), and distance-culled Turkish shop signs (`ECZANE`, `TAŞ FIRIN`, `BAKKAL`, `ÇAY OCAĞI`, `TEKEL BAYİİ`).
- **`ABosphorusWaterVolume`** (`Source/IstanbulTaxi/Environment/BosphorusWaterVolume.h/.cpp`):
  - `64x64` procedural water surface mesh with two-layer Gerstner-like wave displacement, shoreline foam, Bosphorus Ferry (*Şehir Hatları Vapuru*) and *Kız Kulesi* silhouettes, and a submerged safety kill-box that respawns any submerged vehicle onto the nearest coastal road node.

### 2. Refined Vehicle Meshes, 80 m LOD, Chokepoints & Horn System
- **`ATrafficVehicle` & `AIstanbulTrafficManager`** (`Source/IstanbulTaxi/Traffic/IstanbulTrafficManager.h/.cpp`):
  - **5 Istanbul Archetypes (`EIstanbulVehicleType`)**:
    1. `YellowTaxi`: Beveled hood, raked windshield, cabin, trunk, bumpers, wheel arches, side mirrors, roof `TAKSİ` sign (shared with `ATaxiPawn::TaxiBodyProcMesh`).
    2. `IETTBus`: `11.8 m` red/white municipal bus (`#C8102E`), 3 right-side doors, rooftop AC pods, and near-player route `UTextRenderComponent` (`"28T EMİNÖNÜ - BEŞİKTAŞ"`).
    3. `Dolmus`: `5.6 m` yellow Transit-style minibus (`#F2C618`), black beltline stripe, right-side sliding door seam, roof route sign (`"TAKSİM - BEŞİKTAŞ"`).
    4. `DeliveryVan`: `5.2 m` logistics van with white/blue/grey color variants, rear double-door seam, ladder rack, and blinking hazard lamps when double-parked.
    5. `ScooterCourier`: Compact 2-wheel kurye scooter with rider silhouette and rear thermal delivery box (`"KURYE"`).
  - **Per-Instance Variation & 80 m LOD Swap**: `±4%` color/dirt jitter, front/rear Turkish license plate quads (`"34 T 1453"` with near-player `UTextRenderComponent` within `32 m`), front-wheel steering yaw + speed rotation, and automatic swap to a simplified low-poly LOD mesh (`VehicleLodMesh`) beyond `80 m`.
  - **Chokepoints (`FIstanbulChokepointSpec`) & `UHornResponseComponent`**:
    - 4 active narrow-street chokepoints (`2.45–2.80 m` free gap width): Galata slope double-parked van, Sirkeci corner İETT bus, Cihangir ridge loading dolmuş, and Old-Town arched gateway.
    - AI vehicles yield to oncoming traffic, wait `8–15 s`, honk, and reverse/re-route with a global stuck detector.
    - Player horn (`[H]`) supports **short polite tap** (`< 0.35 s`) vs. **long aggressive press** (`>= 0.35 s`). Nearby `UHornResponseComponent` instances (`< 25 m`) evaluate driver `Patience` and `DriverMood` to pull aside/nudge forward, honk back, or tailgate.
- **`ATaxiPawn` & `UPassengerInteractionComponent`** (`Source/IstanbulTaxi/Vehicle/TaxiPawn.h/.cpp`, `Source/IstanbulTaxi/Missions/PassengerInteractionComponent.h/.cpp`):
  - Tuned 1350 kg Chaos / kinematic vehicle (`420 Nm` diesel torque, `0` creep handbrake hold on `18°` grades, light side-scrape impulse damping).
  - Gameplay delegates (`OnChokepointEntered`, `OnHornResponse`, `OnNearMiss`, `OnScrape`) wired to `UPassengerInteractionComponent` so all 4 passenger archetypes react in-character.

---

## Console Commands

| Console Command | Description |
| :--- | :--- |
| `Istanbul.Rebuild` | Destroys all generated scene actors/meshes and deterministically regenerates the city from `ATestSceneBuilder::Seed`. |
| `Istanbul.TeleportToSlope` | Teleports the player taxi to the base of the `17.5°` Cihangir switchback / stop-sign slope. |
| `Istanbul.TeleportToBosphorus` | Teleports the player taxi to the Bosphorus Scenic Overlook / Coastal Quay ledge. |
| `Istanbul.ToggleDebugSlope` | Toggles continuous slope angle (`deg`) and grade (`%`) telemetry logging under the player taxi. |
| `Istanbul.TeleportToChokepoint <index>` | Teleports the player taxi `12 m` in front of active chokepoint `<index>` (`0..3`). |
| `Istanbul.ToggleChokepointDebug` | Toggles 3D debug spheres and free-gap width labels (`2.45–2.80 m`) at all chokepoints. |
| `Istanbul.SpawnTraffic <type> <count>` | Spawns `<count>` live traffic vehicles of `<type>` (`taxi`, `bus`, `dolmus`, `van`, `scooter`). |
| `Istanbul.Stats` | Logs live counts of road segments, districts, buildings, HISM instances, proc-mesh sections, active vehicles by type, 80 m LOD state, and chokepoints. |
