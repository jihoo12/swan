-- Headless play-through: walk to every collectible, collect it, and check the scene's goal.
-- Run: swan script examples/scripts/garden-bot.lua [SCENE]
local scene = swan.args[1] or "assets/scenes/scripted-garden.swan.json"
local sim = swan.simulate(scene)

-- The camera starts facing -Z, so input.move.y walks toward -Z and move.x toward +X.
local function walk_to(target, reach)
  for _ = 1, 600 do
    local offset = target - sim:player().position
    offset.y = 0
    if offset:length() < reach then return true end
    local dir = offset:normalized()
    sim:step(0.05, { move = vec2(dir.x, -dir.z) })
  end
  return false
end

local shards = {}
for _, entity in ipairs(sim:entities()) do
  if entity.collectible then shards[#shards + 1] = entity.key end
end

for _, key in ipairs(shards) do
  local shard = assert(sim:entity(key), key .. " is missing")
  assert(walk_to(shard.world_position, 1.2), "could not reach " .. key)
  sim:step(0.05, { interact = true })
  assert(not sim:entity(key), key .. " was not collected")
end
sim:step(4.0)

for _, line in ipairs(sim:messages()) do print("  script: " .. line) end
print(("collected %d/%d in %.1f s, restored=%s"):format(sim.collected, sim.total, sim.time, tostring(sim.restored)))
assert(sim.restored, "garden was not restored")
local gate = sim:entity("gate")
if gate then
  print(("gate height %.2f"):format(gate.position.y))
  assert(gate.position.y > 4.5, "gate did not open")
end
