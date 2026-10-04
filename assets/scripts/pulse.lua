-- Breathes its entity's scale around the authored size.
---@class Pulse : Behaviour
---@field amount number Relative scale change (0.15 = +/-15%).
---@field rate number Pulses per second, in radians.
---@field base vec3
local Pulse = { properties = { amount = 0.15, rate = 2.0 } }

function Pulse:start()
  self.base = self.entity.scale
end

function Pulse:update(dt)
  self.entity.scale = self.base * (1 + math.sin(game.time * self.rate) * self.amount)
end

return Pulse
