-- Headless play-through of the trigger garden: step on the switch, check that the door opens,
-- step off, and check that it closes again.
-- Run: swan script examples/scripts/trigger-bot.lua [SCENE]
local scene = swan.args[1] or "assets/scenes/trigger-garden.swan.json"
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

local closed = assert(sim:entity("door"), "door is missing").position.y
assert(walk_to(sim:entity("switch").world_position, 0.3), "could not reach the switch")
sim:step(1.5)
local open = sim:entity("door").position.y
print(("door %.2f -> %.2f while on the switch"):format(closed, open))
assert(open > closed + 2.5, "door did not open")

assert(walk_to(vec3(0, 0, 3), 0.3), "could not step off the switch")
sim:step(1.5)
local after = sim:entity("door").position.y
print(("door %.2f after stepping off"):format(after))
assert(math.abs(after - closed) < 0.01, "door did not close")

local opened, closing = false, false
for _, line in ipairs(sim:messages()) do
  print("  script: " .. line)
  opened = opened or line:find("Door opening") ~= nil
  closing = closing or line:find("Door closing") ~= nil
end
assert(opened and closing, "player_enter/player_exit did not fire")
