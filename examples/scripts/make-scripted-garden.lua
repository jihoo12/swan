-- Builds assets/scenes/scripted-garden.swan.json from the glTF garden, entirely through the
-- document command API. Run: swan script examples/scripts/make-scripted-garden.lua
local doc = swan.open("assets/scenes/gltf-garden.swan.json")

doc:transaction("Add behaviours", function()
  for _, name in ipairs({ "spinner", "pulse", "shard", "gate" }) do
    doc:add_script(name, "assets/scripts/" .. name .. ".lua")
  end
  for _, entity in ipairs(doc:entities()) do
    if entity.collectible then doc:set(entity.key, { script = "shard" }) end
  end
  doc:set("pedestal", { script = "pulse", properties = { amount = 0.04, rate = 1.5 } })

  -- A reward alcove behind a gate that opens when every shard is collected.
  doc:create_material("gate", { base = "stone", color = vec3(0.55, 0.6, 0.7) })
  doc:create{ key = "alcove-left", name = "Alcove left", material = "stone", solid = true,
              position = vec3(-2.2, 1.5, -7), scale = vec3(0.4, 3, 2.4) }
  doc:create{ key = "alcove-right", name = "Alcove right", material = "stone", solid = true,
              position = vec3(2.2, 1.5, -7), scale = vec3(0.4, 3, 2.4) }
  doc:create{ key = "alcove-back", name = "Alcove back", material = "stone", solid = true,
              position = vec3(0, 1.5, -8.2), scale = vec3(4.8, 3, 0.4) }
  doc:create{ key = "gate", name = "Gate", material = "gate", solid = true, script = "gate",
              position = vec3(0, 1.5, -5.8), scale = vec3(4, 3, 0.3) }
  doc:create{ key = "reward", name = "Reward", mesh = "crystal-mesh", material = "gold", script = "spinner",
              properties = { speed = 2.5 }, position = vec3(0, 1.2, -7.2), scale = vec3(0.8, 0.8, 0.8) }
end)

doc:save("assets/scenes/scripted-garden.swan.json")
print(("Saved %s with %d entities"):format(doc.path, #doc:entities()))
