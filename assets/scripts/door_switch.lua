-- A trigger volume that opens a door while the player stands inside it.
---@class DoorSwitch : Behaviour
---@field door string Key of the door entity to lift.
---@field rise number How far the door lifts, in meters.
---@field speed number Meters per second.
---@field target Entity?
---@field closed number
local DoorSwitch = { properties = { door = "door", rise = 2.6, speed = 3 } }

function DoorSwitch:start()
  self.target = game.find(self.door)
  self.closed = self.target and self.target.position.y or 0
end

function DoorSwitch:player_enter()
  game.message("Door opening")
end

function DoorSwitch:player_exit()
  game.message("Door closing")
end

function DoorSwitch:update(dt)
  if not (self.target and self.target.alive) then return end
  local goal = self.entity.player_inside and self.closed + self.rise or self.closed
  local position = self.target.position
  local step = self.speed * dt
  position.y = position.y + math.max(-step, math.min(step, goal - position.y))
  self.target.position = position
end

return DoorSwitch
