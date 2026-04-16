--- @meta

---@class TileMap2D : Mesh3D
TileMap2D = {}

---@param worldPos Vector
---@return integer, integer
function TileMap2D:WorldToCell(worldPos) end

---@param cx integer
---@param cy integer
---@return Vector
function TileMap2D:CellToWorld(cx, cy) end

---@param cx integer
---@param cy integer
---@return Vector
function TileMap2D:CellCenterToWorld(cx, cy) end

---@param cx integer
---@param cy integer
---@return integer
function TileMap2D:GetTile(cx, cy) end

---@param cx integer
---@param cy integer
---@param tileIdx integer
function TileMap2D:SetTile(cx, cy, tileIdx) end

---@param cx integer
---@param cy integer
function TileMap2D:ClearTile(cx, cy) end

---@param cx integer
---@param cy integer
---@return any
function TileMap2D:GetCell(cx, cy) end

---@param cx integer
---@param cy integer
---@return boolean
function TileMap2D:IsCellOccupied(cx, cy) end

---@param tileIdx integer
---@return string
function TileMap2D:GetTileTags(tileIdx) end

---@param tileIdx integer
---@param tag string
---@return boolean
function TileMap2D:HasTileTag(tileIdx, tag) end

---@param tileIdx integer
---@return integer, integer
function TileMap2D:GetTileAtlasCoord(tileIdx) end

---@param tag string
---@return integer
function TileMap2D:FindAllTilesWithTag(tag) end

---@param targetTileIdx integer
---@return integer
function TileMap2D:FindAllTiles(targetTileIdx) end

---@return integer, integer, integer, integer
function TileMap2D:GetUsedBounds() end

---@param worldPos Vector
---@return boolean
function TileMap2D:IsSolidAt(worldPos) end

---@param cx integer
---@param cy integer
---@return boolean
function TileMap2D:IsSolidCell(cx, cy) end

---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param tileIdx integer
function TileMap2D:FillRect(x1, y1, x2, y2, tileIdx) end

---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function TileMap2D:ClearRect(x1, y1, x2, y2) end

---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@return integer
function TileMap2D:GetCellsInRect(x1, y1, x2, y2) end

---@param worldPos Vector
---@return nil
function TileMap2D:GetClosestTile(worldPos) end

---@param tag string
---@param worldPos Vector
---@param maxDist? number
---@return any
function TileMap2D:GetClosestTilesWithTag(tag, worldPos, maxDist) end

---@param targetTile integer
---@param worldPos Vector
---@param maxDist? number
function TileMap2D:GetClosestTilesOfType(targetTile, worldPos, maxDist) end

---@param cellX integer
---@param cellY integer
---@return integer
function TileMap2D:GetNeighborCells(cellX, cellY) end

---@param worldPos Vector
---@return table
function TileMap2D:GetCollisionAt(worldPos) end

---@param oldIdx integer
---@param newIdx integer
---@return integer
function TileMap2D:ReplaceTile(oldIdx, newIdx) end

---@param tag string
---@param newIdx integer
---@return integer
function TileMap2D:ReplaceTilesWithTag(tag, newIdx) end

---@param tileIdx integer
---@return integer
function TileMap2D:CountTileUses(tileIdx) end

---@param start Vector
---@param end Vector
---@return table
function TileMap2D:RaycastTiles(start, end) end

---@param startX integer
---@param startY integer
---@param maxDistance integer
---@return integer
function TileMap2D:GetReachableCells(startX, startY, maxDistance) end
