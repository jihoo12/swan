-- Turns its entity around the vertical axis.
---@class Spinner : Behaviour
---@field speed number Radians per second (entity property).
local Spinner = { properties = { speed = 1.0 } }

function Spinner:update(dt)
  self.entity.yaw = self.entity.yaw + self.speed * dt
end

return Spinner
