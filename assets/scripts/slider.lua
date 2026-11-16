-- Slides back and forth along Z.
---@class Slider : Behaviour
---@field distance number Half the travel, in meters.
---@field rate number Radians per second.
---@field base vec3
local Slider = { properties = { distance = 3, rate = 0.9 } }

function Slider:start()
  self.base = self.entity.position
end

function Slider:update()
  self.entity.position = self.base + vec3(0, 0, math.sin(game.time * self.rate) * self.distance)
end

return Slider
