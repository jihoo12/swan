---@meta
-- Type definitions for Swan's Lua API, for lua-language-server (LuaLS).
-- The repository's .luarc.json loads this file; installed copies live in share/swan/lua/types.

---@class vec3
---@field x number
---@field y number
---@field z number
---@operator add(vec3): vec3
---@operator sub(vec3): vec3
---@operator mul(number|vec3): vec3
---@operator div(number): vec3
---@operator unm: vec3
local vec3_methods = {}
---@return number
function vec3_methods:length() end
---@return vec3
function vec3_methods:normalized() end
---@param other vec3
---@return number
function vec3_methods:dot(other) end
---@param other vec3
---@return vec3
function vec3_methods:cross(other) end
---@param other vec3
---@return number
function vec3_methods:distance(other) end
---@param other vec3
---@param t number
---@return vec3
function vec3_methods:lerp(other, t) end

---Create a vector: `vec3()`, `vec3(s)`, `vec3(x, y, z)`, or `vec3{x, y, z}`.
---@overload fun(): vec3
---@overload fun(s: number): vec3
---@overload fun(t: number[]|{x: number, y: number, z: number}): vec3
---@param x number
---@param y number
---@param z number
---@return vec3
function vec3(x, y, z) end

---@class vec2
---@field x number
---@field y number
---@operator add(vec2): vec2
---@operator sub(vec2): vec2
---@operator mul(number): vec2
---@operator unm: vec2
local vec2_methods = {}
---@return number
function vec2_methods:length() end

---@overload fun(): vec2
---@overload fun(s: number): vec2
---@overload fun(t: number[]|{x: number, y: number}): vec2
---@param x number
---@param y number
---@return vec2
function vec2(x, y) end

-------------------------------------------------------------------------------------------------
-- Gameplay behaviours (scene `scripts`, sandboxed: no io/os/require)
-------------------------------------------------------------------------------------------------

---A live runtime entity. Accessing a destroyed entity raises an error; check `alive`.
---@class Entity
---@field key string Stable ID from the scene file (read-only).
---@field name string
---@field position vec3 Local position; assign a whole vec3 (getters return copies).
---@field scale vec3 Positive; uniform when the entity has children.
---@field yaw number Radians around +Y.
---@field world_position vec3 Read-only.
---@field material string Material asset ID.
---@field parent Entity? Read-only.
---@field script string Read-only.
---@field effect string? Attached particle effect ID; assign nil to stop emitting (live particles finish).
---@field alive boolean
local Entity = {}
---Remove the entity; direct children keep their world placement.
function Entity:destroy() end

---A behaviour script returns a table of callbacks. Each scripted entity gets its own `self`,
---which holds the `properties` defaults, the entity's overrides, and `self.entity`.
---Each callback has an instruction budget; an error stops only that entity's behaviour.
---@class Behaviour
---@field entity Entity
---@field properties? table<string, number|boolean|string> Defaults, overridable per entity.
---@field start? fun(self: Behaviour) Called once before the first update.
---@field update? fun(self: Behaviour, dt: number) Called every fixed tick (1/120 s).
---@field collected? fun(self: Behaviour) Called when the player collects this entity.

---@class GamePlayer
---@field position vec3 Feet position.
---@field grounded boolean
---@field flying boolean

---@class GameInput
---@field move vec2
---@field jump boolean
---@field interact boolean
---@field sprint boolean

---@class SpawnSpec
---@field name? string
---@field mesh? string Defaults to "builtin:cube".
---@field material? string Defaults to "default".
---@field position? vec3|number[]
---@field scale? vec3|number[]
---@field yaw? number
---@field script? string Starts on the next tick.
---@field effect? string Attached particle effect ID.
---@field properties? table<string, number|boolean|string>

---Global game state, refreshed every tick (behaviour scripts only).
---@class Game
---@field time number Seconds of unpaused play.
---@field collected integer
---@field total integer Collectibles in the scene.
---@field player GamePlayer
---@field input GameInput
game = {}
---Show text in the status line and log it.
---@param text string
function game.message(text) end
---@param key string
---@return Entity?
function game.find(key) end
---@return Entity[]
function game.entities() end
---@param spec SpawnSpec
---@return Entity
function game.spawn(spec) end

---@class EffectPlayOptions
---@field position? vec3|number[] World position, or an offset from `entity`.
---@field entity? Entity|string Follow this entity.
---@field yaw? number Radians.
---@field seed? integer

---Play a one-shot instance of a scene effect (it removes itself when finished).
---@param id string Effect asset ID.
---@param options? EffectPlayOptions
function game.effect(id, options) end

-------------------------------------------------------------------------------------------------
-- Automation (`swan script FILE`)
-------------------------------------------------------------------------------------------------

---Snapshot of an entity (a plain table; edit through Document methods).
---@class EntitySnapshot
---@field key string
---@field name string
---@field mesh string
---@field material string
---@field position vec3
---@field scale vec3
---@field yaw number
---@field world_position vec3
---@field solid boolean
---@field collectible boolean
---@field goal boolean
---@field parent string?
---@field script string?
---@field effect string?
---@field properties table<string, number|boolean|string>

---@class EntitySpec : SpawnSpec
---@field key? string Stable ID; generated when omitted.
---@field solid? boolean
---@field collectible? boolean
---@field goal? boolean
---@field parent? string

---@class EntityChanges
---@field name? string
---@field mesh? string
---@field material? string
---@field position? vec3|number[]
---@field scale? vec3|number[]
---@field yaw? number
---@field solid? boolean
---@field collectible? boolean
---@field goal? boolean
---@field script? string|false `false` removes the behaviour.
---@field effect? string|false Attach a particle effect; `false` detaches it.
---@field properties? table<string, number|boolean|string> Replaces all overrides.
---@field parent? string|false Keeps the local transform; see Document:reparent().

---@class MaterialSpec
---@field base? string Copy this material first (create_material only).
---@field color? vec3|number[]
---@field emission? number
---@field texture? string
---@field uv_scale? vec2|number[]

-------------------------------------------------------------------------------------------------
-- Effects and timelines (see docs/FX.md for every field and its default)
-------------------------------------------------------------------------------------------------

---@alias Ease "linear"|"step"|"smooth"|"in"|"out"|"ease_in"|"ease_out"|"ease_in_out"
---A keyframe: `{ time, value }`, `{ time, value, ease }`, or `{ t = time, value = v, ease = e }`.
---Values are numbers, `vec3`/`{x, y, z}`, or colors `{r, g, b[, a]}`.
---@alias Keyframe any[]|{t: number, value: any, ease: Ease?}
---A curve: a list of keyframes, or a bare constant value.
---@alias Curve Keyframe[]|number|number[]|vec3
---A number, or a uniform random range `{ min, max }`.
---@alias Range number|number[]
---@alias Color number[] `{ r, g, b }` or `{ r, g, b, a }`, linear, alpha 0..1.

---@class EmitterSpec
---@field name? string
---@field offset? vec3|number[] From the effect origin, rotated by its yaw.
---@field delay? number Seconds before the emission window starts.
---@field duration? number Emission window length (default 1).
---@field loop? boolean Repeat the window (rate curve and bursts) forever.
---@field rate? number Particles per second.
---@field rate_over_time? Curve Multiplier over the normalized window (0..1).
---@field bursts? ({time: number, count: integer}|number[])[] `{ time, count }` within the window.
---@field lifetime? Range Seconds (default 1).
---@field shape? "point"|"sphere"|"box"|"ring"|"disc"
---@field radius? number Sphere, ring, and disc (default 0.5).
---@field box? vec3|number[] Box extents.
---@field surface? boolean Spawn on the surface/edge only.
---@field direction? vec3|number[] Emission axis (default up); also orients ring/disc and orbit.
---@field spread? number Cone half-angle in degrees, 0..180.
---@field radial? boolean Aim outward from the emitter center.
---@field speed? Range
---@field velocity? vec3|number[] Added to each new particle.
---@field gravity? vec3|number[] Acceleration.
---@field drag? number Exponential damping per second.
---@field noise? number Turbulent acceleration strength.
---@field noise_frequency? number
---@field orbit? number Radians per second around the emitter axis.
---@field size? Range Start size in world units (default 0.2).
---@field size_over_life? Curve Multiplier over normalized age.
---@field color? Color Base color (default white).
---@field color_over_life? Curve Color multiplier over normalized age.
---@field intensity? number HDR brightness; values above ~1 bloom.
---@field rotation? Range Degrees.
---@field spin? Range Degrees per second.
---@field stretch? number Elongate along on-screen velocity.
---@field blend? "additive"|"alpha"
---@field sprite? "soft"|"circle"|"ring"|"square"|"spark"|"smoke"
---@field texture? string Texture asset ID multiplied with the sprite.
---@field space? "world"|"local" `local` particles move with the emitter.
---@field max_particles? integer 1..20000 (default 1000).
---@field seed? integer

---@class EffectSpec
---@field emitters EmitterSpec[] 1..16 emitters.

---@class TrackSpec
---@field entity? string Entity key (properties: position, scale, yaw, emit).
---@field material? string Material ID (properties: color, emission).
---@field environment? boolean `true` (properties: exposure, bloom, background).
---@field property string
---@field keys? Curve Omit (or `{}`) to remove the track.

---@class EventSpec
---@field time number Seconds on the timeline.
---@field effect string
---@field entity? string Follow this entity; `position` becomes an offset.
---@field position? vec3|number[]
---@field yaw? number
---@field seed? integer

---@class CameraSpec
---@field position? Curve
---@field target? Curve
---@field fov? Curve Degrees.

---@class TimelineSpec
---@field duration? number 0: until the last key or event.
---@field loop? boolean
---@field camera? CameraSpec
---@field tracks? TrackSpec[]
---@field events? EventSpec[]

---@class EnvironmentSpec
---@field background? number[] Linear clear and fog color.
---@field fog? number Density over squared distance.
---@field ambient? number
---@field sun? number
---@field local_light? number
---@field exposure? number
---@field bloom? number Bloom strength (0 disables).
---@field bloom_threshold? number
---@field tonemap? "reinhard"|"aces"

---A scene being edited. Every call is a validated, undoable document command.
---@class Document
---@field path string
---@field modified boolean
---@field selection string?
local Document = {}
---@return EntitySnapshot[]
function Document:entities() end
---@param key string
---@return EntitySnapshot?
function Document:entity(key) end
---@param spec? EntitySpec
---@return string key
function Document:create(spec) end
---@param key string
---@param changes EntityChanges
function Document:set(key, changes) end
---Change the parent while keeping the world placement.
---@param key string
---@param parent? string
function Document:reparent(key, parent) end
---@param key string
function Document:delete(key) end
---@param key? string
function Document:select(key) end
---@return table<string, MaterialSpec>
function Document:materials() end
---@param id string
---@param changes MaterialSpec
function Document:set_material(id, changes) end
---@param id string
---@param spec? MaterialSpec
function Document:create_material(id, spec) end
---@return table<string, string> id -> source path
function Document:scripts() end
---@param id string
---@param path string
function Document:add_script(id, path) end
---@return boolean
function Document:undo() end
---@return boolean
function Document:redo() end
---@return {undo: string[], redo: string[]}
function Document:history() end
---Run `body` as a single undo step; an error inside rolls its edits back and propagates.
---@param label string
---@param body fun()
function Document:transaction(label, body) end
---@param path? string Defaults to the opened path.
function Document:save(path) end
---@return table<string, EffectSpec>
function Document:effects() end
---@param id string
---@return EffectSpec?
function Document:effect(id) end
---Create or replace an effect. Errors name the emitter and field.
---@param id string
---@param spec EffectSpec
function Document:set_effect(id, spec) end
---Delete an effect, detaching it from entities and removing timeline events that play it.
---@param id string
function Document:delete_effect(id) end
---The timeline, always with `tracks` and `events` arrays and the effective `duration`.
---@return TimelineSpec
function Document:timeline() end
---Replace the whole timeline (nil clears it).
---@param spec? TimelineSpec
function Document:set_timeline(spec) end
---Replace the track for one target and property; without `keys` it is removed.
---@param spec TrackSpec
function Document:set_track(spec) end
---@param spec EventSpec
function Document:add_event(spec) end
---Set the shot camera (nil clears it).
---@param spec? CameraSpec
function Document:set_camera(spec) end
---@return EnvironmentSpec
function Document:environment() end
---Merge changes into the scene environment.
---@param changes EnvironmentSpec
function Document:set_environment(changes) end

---@class SimulationInput
---@field move? vec2|number[]
---@field look? vec2|number[]
---@field vertical? number
---@field sprint? boolean
---@field jump? boolean One-shot: first tick of the step only.
---@field interact? boolean One-shot.
---@field pause? boolean
---@field reset? boolean
---@field toggle_flight? boolean
---@field toggle_camera? boolean

---A windowless game run at the fixed 120 Hz tick, including the scene's behaviour scripts.
---@class Simulation
---@field time number
---@field ticks integer
---@field collected integer
---@field total integer
---@field restored boolean
---@field status string
local Simulation = {}
---@param seconds number
---@param input? SimulationInput
---@return integer ticks
function Simulation:step(seconds, input) end
---@param input? SimulationInput
function Simulation:tick(input) end
---@return GamePlayer
function Simulation:player() end
---@param key string
---@return EntitySnapshot?
function Simulation:entity(key) end
---@return EntitySnapshot[]
function Simulation:entities() end
---Script output and errors since the last call.
---@return string[]
function Simulation:messages() end

---@class FxEmitterStats
---@field effect string
---@field emitter string Name, or index when unnamed.
---@field entity string? Followed entity.
---@field particles integer
---@field spawned integer
---@field dropped integer Spawns refused by max_particles.
---@field min vec3? World bounds of live particles.
---@field max vec3?

---@class FxStats
---@field time number
---@field particles integer
---@field instances integer
---@field spawned integer
---@field dropped integer
---@field bounds {min: vec3, max: vec3}?
---@field emitters FxEmitterStats[]

---@class ParticleSnapshot
---@field position vec3
---@field size number
---@field color number[] `{ r, g, b, a }` (RGB includes intensity).
---@field velocity vec3
---@field blend "additive"|"alpha"

---@class CameraState
---@field position vec3
---@field target vec3
---@field fov number
---@field yaw number
---@field pitch number

---@class RenderCamera
---@field position vec3|number[]
---@field target? vec3|number[]
---@field fov? number
---@field yaw? number Used without a target.
---@field pitch? number

---@class RenderOptions
---@field width? integer Default 640 (320 per sheet frame).
---@field height? integer Default 360 (180 per sheet frame).
---@field supersample? integer 1..4 (default 2).
---@field camera? RenderCamera Default: the timeline camera, else automatic framing.

---@class SheetOptions : RenderOptions
---@field from? number Default 0.
---@field to? number Default: the timeline length, or 2 s.
---@field count? integer Frames, 1..64 (default 8).
---@field columns? integer Default 4.
---@field labels? boolean Time and particle count per frame (default true).

---A deterministic, windowless preview of a scene's effects and timeline (120 Hz ticks).
---@class Preview
---@field time number
---@field ticks integer
---@field duration number Timeline length (0 without a timeline).
---@field timeline_time number
---@field particle_count integer
---@field can_render boolean True inside `swan script` (headless Vulkan).
local Preview = {}
---@param seconds number
---@return integer ticks
function Preview:step(seconds) end
---Jump to a time; seeking backwards replays from the start (same result every time).
---@param seconds number
function Preview:seek(seconds) end
function Preview:restart() end
---@return FxStats
function Preview:stats() end
---@param key string
---@return EntitySnapshot?
function Preview:entity(key) end
---@return EntitySnapshot[]
function Preview:entities() end
---@param id string
---@return MaterialSpec?
function Preview:material(id) end
---@param limit? integer Default 1000.
---@return ParticleSnapshot[]
function Preview:particles(limit) end
---@return CameraState
function Preview:camera() end
---@param effect string
---@param options? {position?: vec3|number[], entity?: string, yaw?: number, seed?: integer}
function Preview:play(effect, options) end
---Behaviour script output and errors since the last call.
---@return string[]
function Preview:messages() end
---Render the current time to a PNG (needs `swan script`).
---@param path string
---@param options? RenderOptions
---@return string path
function Preview:render(path, options) end
---Render evenly spaced times into one labeled PNG contact sheet.
---@param path string
---@param options? SheetOptions
---@return string path
function Preview:render_sheet(path, options) end

---@class swan
---@field version string
---@field args string[] Arguments after the script path.
swan = {}
---@param path string
---@return Document
function swan.open(path) end
---@return Document
function swan.new() end
---@param path string
---@return boolean ok
---@return string? error
function swan.validate(path) end
---@param scene Document|string
---@param options? {third_person?: boolean, flying?: boolean}
---@return Simulation
function swan.simulate(scene, options) end
---@param scene Document|string
---@param options? {seed?: integer, scripts?: boolean}
---@return Preview
function swan.preview(scene, options) end
