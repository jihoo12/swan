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

-------------------------------------------------------------------------------------------------
-- Automation (`swan script FILE`, the editor's Lua console)
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
---@field properties? table<string, number|boolean|string> Replaces all overrides.
---@field parent? string|false Keeps the local transform; see Document:reparent().

---@class MaterialSpec
---@field base? string Copy this material first (create_material only).
---@field color? vec3|number[]
---@field emission? number
---@field texture? string
---@field uv_scale? vec2|number[]

---A scene being edited. Every call is a validated, undoable editor command.
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

---The editor console's live document (only defined in the editor's Lua console).
---@type Document
doc = nil
