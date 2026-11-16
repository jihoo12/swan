-- A light beam that raycasts every tick and stops at the first thing in its way.
---@class Beam : Behaviour
---@field range number Maximum beam length, in meters.
---@field origin vec3
---@field hot boolean
local Beam = { properties = { range = 20 } }

function Beam:start()
  -- The beam starts at its authored left end and points along +X.
  self.origin = self.entity.position - vec3(self.entity.scale.x / 2, 0, 0)
end

function Beam:update()
  local hit = game.raycast(self.origin, vec3(1, 0, 0), self.range, { ignore = self.entity })
  local length = hit and hit.distance or self.range
  local scale = self.entity.scale
  scale.x = math.max(length, 0.01)
  self.entity.scale = scale
  self.entity.position = self.origin + vec3(length / 2, 0, 0)
  -- Turn red while the moving pillar blocks the beam.
  local hot = hit ~= nil and hit.entity.key == "slider"
  if hot ~= self.hot then
    self.hot = hot
    self.entity.material = hot and "beam-blocked" or "beam"
  end
end

return Beam
