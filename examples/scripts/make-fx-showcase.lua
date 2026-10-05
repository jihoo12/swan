-- Builds assets/scenes/fx-showcase.swan.json entirely through the automation API: a campfire,
-- a portal, and a magic orb that flies out of the portal and explodes over the fire.
--   swan script examples/scripts/make-fx-showcase.lua [OUTPUT.swan.json] [SHEET.png]
-- With a sheet path, it also renders a labeled contact sheet of the shot (needs a GPU).
local output = swan.args[1] or "assets/scenes/fx-showcase.swan.json"
local sheet = swan.args[2]
local doc = swan.new()

doc:transaction("Build FX showcase", function()
  doc:set_environment{ background = { 0.012, 0.014, 0.025 }, fog = 0.002, ambient = 0.12, sun = 0.25,
                       local_light = 0, exposure = 1.1, bloom = 0.9, bloom_threshold = 0.9, tonemap = "aces" }
  doc:create_material("ground", { color = vec3(0.12, 0.11, 0.1) })
  doc:create_material("stone", { color = vec3(0.35, 0.36, 0.4) })
  doc:create_material("wood", { color = vec3(0.25, 0.14, 0.08) })
  doc:create_material("orb", { color = vec3(0.6, 0.8, 1.0), emission = 4 })
  doc:create{ key = "ground", name = "Ground", material = "ground", position = vec3(0, -0.5, 0), scale = vec3(30, 1, 30) }
  for i, yaw in ipairs({ 0.3, 1.9, 3.4 }) do
    doc:create{ key = "log-" .. i, name = "Log " .. i, material = "wood", position = vec3(0, 0.08, 0),
                scale = vec3(1.1, 0.16, 0.18), yaw = yaw }
  end
  doc:create{ key = "portal-base", name = "Portal base", material = "stone", position = vec3(-4, 0.1, -3), scale = vec3(2.8, 0.2, 0.8) }

  -- Campfire: additive flames, stretched embers, and alpha-blended smoke above them.
  doc:set_effect("campfire", { emitters = {
    { name = "flames", loop = true, duration = 1, rate = 55, lifetime = { 0.5, 0.9 }, shape = "disc", radius = 0.3,
      spread = 8, speed = { 1.0, 1.7 }, size = { 0.22, 0.4 }, size_over_life = { { 0, 0.5 }, { 0.25, 1 }, { 1, 0.15, "in" } }, stretch = 0.35,
      color_over_life = { { 0, { 1, 0.85, 0.45, 0 } }, { 0.1, { 1, 0.55, 0.12, 0.9 } }, { 0.6, { 0.9, 0.22, 0.04, 0.6 } }, { 1, { 0.4, 0.05, 0.01, 0 } } },
      intensity = 1.6, noise = 1.4, noise_frequency = 2 },
    { name = "core", loop = true, duration = 1, rate = 12, lifetime = 0.4, shape = "disc", radius = 0.12, speed = 0.5,
      size = 0.5, color = { 1, 0.7, 0.3, 0.35 }, intensity = 1.2, offset = { 0, 0.15, 0 } },
    { name = "embers", loop = true, duration = 1, rate = 18, lifetime = { 1.2, 2.4 }, shape = "disc", radius = 0.3, spread = 20,
      speed = { 1.8, 3.2 }, size = 0.05, sprite = "spark", stretch = 0.12, color = { 1, 0.55, 0.15 }, intensity = 8,
      color_over_life = { { 0, { 1, 1, 1, 1 } }, { 1, { 1, 0.4, 0.2, 0 } } }, noise = 2.5, drag = 0.5 },
    { name = "smoke", loop = true, duration = 1, rate = 9, lifetime = { 2.5, 3.5 }, offset = { 0, 1.1, 0 }, shape = "disc",
      radius = 0.2, spread = 8, speed = { 0.5, 0.8 }, size = { 0.6, 0.9 }, size_over_life = { { 0, 0.6 }, { 1, 2.6 } },
      blend = "alpha", sprite = "smoke", color = { 0.16, 0.15, 0.15 },
      color_over_life = { { 0, { 1, 1, 1, 0 } }, { 0.2, { 1, 1, 1, 0.55 } }, { 1, { 1, 1, 1, 0 } } }, noise = 0.4, spin = { -30, 30 } },
  } })
  doc:create{ key = "campfire", name = "Campfire", position = vec3(0, 0.12, 0), scale = vec3(0.1, 0.1, 0.1),
              material = "wood", effect = "campfire" }

  -- Portal: a swirling ring of sparks orbiting its axis, plus an inner glow.
  doc:set_effect("portal", { emitters = {
    { name = "ring", loop = true, duration = 1, rate = 200, lifetime = { 0.8, 1.2 }, shape = "ring", radius = 1.1, direction = { 0, 0, 1 },
      speed = { 0.05, 0.2 }, spread = 180, orbit = 2.5, size = { 0.05, 0.1 },
      color_over_life = { { 0, { 0.5, 0.3, 1, 0 } }, { 0.2, { 0.6, 0.4, 1, 1 } }, { 1, { 0.2, 0.6, 1, 0 } } }, intensity = 5 },
    { name = "swirl", loop = true, duration = 1, rate = 60, lifetime = 1.4, shape = "disc", radius = 1, direction = { 0, 0, 1 }, orbit = -1.5,
      speed = { 0.2, 0.5 }, size = { 0.25, 0.45 }, size_over_life = { { 0, 0 }, { 0.3, 1 }, { 1, 0 } },
      color = { 0.35, 0.2, 0.9, 0.35 }, intensity = 2, noise = 0.6 },
  } })
  doc:create{ key = "portal", name = "Portal", position = vec3(-4, 1.35, -3), scale = vec3(0.1, 0.1, 0.1), material = "stone", effect = "portal" }

  -- The orb trails sparkles; the explosion is a one-shot played by the timeline.
  doc:set_effect("orb-trail", { emitters = {
    { name = "trail", loop = true, duration = 1, rate = 90, lifetime = { 0.4, 0.8 }, shape = "sphere", radius = 0.12, speed = { 0.1, 0.4 },
      spread = 180, size = { 0.08, 0.16 }, size_over_life = { { 0, 1 }, { 1, 0 } }, color = { 0.55, 0.8, 1 }, intensity = 4, drag = 1 },
    { name = "halo", loop = true, duration = 1, rate = 30, lifetime = 0.25, space = "local", size = 0.9, speed = 0,
      color = { 0.4, 0.7, 1, 0.5 }, intensity = 2 },
  } })
  doc:set_effect("explosion", { emitters = {
    { name = "flash", bursts = { { time = 0, count = 1 } }, duration = 0.1, lifetime = 0.2, speed = 0, size = 2.4,
      size_over_life = { { 0, 0.5 }, { 1, 1.4, "out" } }, color_over_life = { { 0, { 1, 0.9, 0.7, 1 } }, { 1, { 1, 0.5, 0.2, 0 } } }, intensity = 6 },
    { name = "sparks", bursts = { { time = 0, count = 220 } }, duration = 0.1, lifetime = { 0.6, 1.4 }, shape = "sphere", radius = 0.1,
      radial = true, spread = 20, speed = { 4, 9 }, gravity = { 0, -6, 0 }, drag = 1.2, size = { 0.04, 0.08 }, sprite = "spark",
      stretch = 0.08, color_over_life = { { 0, { 1, 0.9, 0.6, 1 } }, { 0.5, { 1, 0.5, 0.15, 1 } }, { 1, { 0.8, 0.2, 0.05, 0 } } }, intensity = 7 },
    { name = "shockwave", bursts = { { time = 0, count = 1 } }, duration = 0.1, lifetime = 0.5, speed = 0, size = 1, sprite = "ring",
      size_over_life = { { 0, 0.2 }, { 1, 6, "out" } }, color_over_life = { { 0, { 0.6, 0.8, 1, 0.9 } }, { 1, { 0.3, 0.5, 1, 0 } } }, intensity = 3 },
    { name = "smoke", bursts = { { time = 0, count = 24 } }, duration = 0.1, lifetime = { 1.5, 2.5 }, shape = "sphere", radius = 0.3,
      radial = true, speed = { 0.6, 1.4 }, drag = 1.5, gravity = { 0, 0.4, 0 }, size = { 0.8, 1.3 }, size_over_life = { { 0, 0.5 }, { 1, 2 } },
      blend = "alpha", sprite = "smoke", color = { 0.12, 0.11, 0.11 },
      color_over_life = { { 0, { 1, 1, 1, 0.8 } }, { 1, { 1, 1, 1, 0 } } }, spin = { -45, 45 } },
  } })
  doc:create{ key = "orb", name = "Orb", material = "orb", position = vec3(-4, 1.2, -3), scale = vec3(0.18, 0.18, 0.18), effect = "orb-trail" }
end)

-- A 6-second looping shot: the orb rises from the portal, arcs over the fire, and bursts.
doc:set_timeline{ duration = 6, loop = true }
doc:set_track{ entity = "orb", property = "position", keys = {
  { 0, vec3(-4, 0.3, -3), "out" }, { 1.2, vec3(-4, 2.2, -3), "smooth" }, { 2.6, vec3(-1.2, 3.2, -1), "smooth" }, { 3.4, vec3(0, 2.4, 0) },
  { 3.41, vec3(0, -3, 0), "step" }, { 6, vec3(0, -3, 0) } } }
doc:set_track{ entity = "orb", property = "emit", keys = { { 0, 1 }, { 3.39, 1, "step" }, { 3.4, 0 } } }
doc:add_event{ time = 3.4, effect = "explosion", position = vec3(0, 2.4, 0) }
doc:set_track{ environment = true, property = "exposure", keys = { { 0, 1.1 }, { 3.4, 1.1 }, { 3.45, 1.7, "out" }, { 4.2, 1.1 } } }
doc:set_track{ material = "orb", property = "emission", keys = { { 0, 2 }, { 3.2, 8 } } }
doc:set_camera{ position = { { 0, vec3(3, 2.2, 8), "smooth" }, { 6, vec3(6.5, 3.2, 5.5) } },
                target = { { 0, vec3(-1.5, 1.2, -1), "smooth" }, { 3.4, vec3(0, 1.8, 0), "smooth" }, { 6, vec3(0, 1.2, 0) } },
                fov = 50 }

doc:save(output)
print(("Saved %s: %d entities, timeline %.1f s"):format(output, #doc:entities(), doc:timeline().duration))

if sheet then
  local preview = swan.preview(doc)
  preview:render_sheet(sheet, { from = 0, to = 5.5, count = 12, columns = 4, width = 400, height = 225 })
  print("Rendered " .. sheet)
end
