-- Announces progress when the player collects this shard.
---@class Shard : Behaviour
local Shard = {}

function Shard:collected()
  -- Runs before the shard is removed and counted.
  local left = game.total - game.collected - 1
  if left > 0 then
    game.message(("%d shard%s left"):format(left, left == 1 and "" or "s"))
  else
    game.message("The gate is opening...")
  end
end

return Shard
