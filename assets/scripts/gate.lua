-- A solid gate that slides up once every collectible has been gathered.
---@class Gate : Behaviour
---@field rise number How far the gate lifts, in meters.
---@field speed number Lift speed, in meters per second.
---@field closed number
local Gate = { properties = { rise = 3.4, speed = 1.2 } }

function Gate:start()
  self.closed = self.entity.position.y
end

function Gate:update(dt)
  if game.total == 0 or game.collected < game.total then return end
  local position = self.entity.position
  local open = self.closed + self.rise
  if position.y < open then
    position.y = math.min(open, position.y + self.speed * dt)
    self.entity.position = position
  end
end

return Gate
