-- Prints a table of a scene's entities, materials, and scripts.
-- Run: swan script examples/scripts/scene-report.lua SCENE
local path = assert(swan.args[1], "usage: swan script scene-report.lua SCENE")
local ok, err = swan.validate(path)
if not ok then error(err) end
local doc = swan.open(path)

print(("%-16s %-22s %-14s %-10s %s"):format("ID", "NAME", "MESH", "MATERIAL", "SCRIPT"))
for _, e in ipairs(doc:entities()) do
  print(("%-16s %-22s %-14s %-10s %s"):format(e.key, e.name, e.mesh, e.material, e.script or "-"))
end
local materials, scripts = 0, 0
for _ in pairs(doc:materials()) do materials = materials + 1 end
for _ in pairs(doc:scripts()) do scripts = scripts + 1 end
print(("%d entities, %d materials, %d scripts"):format(#doc:entities(), materials, scripts))
