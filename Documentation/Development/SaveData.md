# Save Data

The SaveData system provides a key-value save system for persisting game data across sessions. It works like Unity's PlayerPrefs: set typed values by string key, save them to a named slot, and load them back later. Supports multiple save slots.

## Platform Support

| Platform         | Storage Location                        | Notes |
|------------------|-----------------------------------------|-------|
| Windows          | `ProjectDir/Saves/`                     | Filesystem |
| Linux            | `ProjectDir/Saves/`                     | Filesystem |
| Android          | `ProjectDir/Saves/`                     | Internal storage |
| GameCube         | Memory Card Slot A                      | Uses CARD API |
| Wii              | SD Card `ProjectDir/Saves/`             | Filesystem |
| 3DS              | `ProjectDir/Saves/`                     | Internal storage |

## Script Example

Create a `Scripts/SaveGameManager.lua`:

```lua
SaveGameManager = {}

function SaveGameManager:Create()
    self.currentSlot = "slot1"
end

function SaveGameManager:Start()
    -- Load existing save if available
    if SaveData.DoesSaveExist(self.currentSlot) then
        SaveData.Load(self.currentSlot)
        Log.Debug("Loaded save: " .. self.currentSlot)
        Log.Debug("  Player: " .. SaveData.GetString("playerName", "Unknown"))
        Log.Debug("  Health: " .. SaveData.GetInt("health", 100))
    else
        -- Initialize defaults for a new game
        SaveData.SetString("playerName", "Hero")
        SaveData.SetInt("health", 100)
        SaveData.SetFloat("playtime", 0.0)
        SaveData.SetVector("spawnPoint", Vec(0, 1, 0))
        SaveData.SetBool("tutorialDone", false)
        SaveData.Save(self.currentSlot)
        Log.Debug("Created new save: " .. self.currentSlot)
    end
end

function SaveGameManager:SaveGame()
    -- Update playtime before saving
    local playtime = SaveData.GetFloat("playtime", 0.0)
    SaveData.SetFloat("playtime", playtime + Engine.GetElapsedTime())

    -- Save the player's current position
    local player = self:GetParent()
    if player then
        SaveData.SetVector("position", player:GetWorldPosition())
    end

    SaveData.Save(self.currentSlot)
    Log.Debug("Game saved to " .. self.currentSlot)
end

function SaveGameManager:Tick(deltaTime)
    -- Save on F5
    if Input.IsKeyJustDown(Key.F5) then
        self:SaveGame()
    end

    -- Load on F9
    if Input.IsKeyJustDown(Key.F9) then
        if SaveData.DoesSaveExist(self.currentSlot) then
            SaveData.Load(self.currentSlot)
            Log.Debug("Game loaded from " .. self.currentSlot)
        end
    end
end
```

## Multiple Save Slots

```lua
-- Save to different slots
function SaveSlotManager:SaveToSlot(slotNumber)
    -- Set up the data you want to save
    SaveData.SetInt("health", self.health)
    SaveData.SetString("playerName", self.playerName)
    SaveData.SetVector("position", self:GetWorldPosition())
    SaveData.SetFloat("playtime", self.playtime)

    -- Save to the slot
    local slotName = "slot" .. tostring(slotNumber)
    SaveData.Save(slotName)
end

function SaveSlotManager:LoadFromSlot(slotNumber)
    local slotName = "slot" .. tostring(slotNumber)

    if SaveData.DoesSaveExist(slotName) then
        SaveData.Load(slotName)

        self.health = SaveData.GetInt("health", 100)
        self.playerName = SaveData.GetString("playerName", "Hero")
        self.playtime = SaveData.GetFloat("playtime", 0.0)

        local pos = SaveData.GetVector("position", Vec(0, 0, 0))
        self:SetWorldPosition(pos)

        return true
    end

    return false
end

-- List available save slots
function SaveSlotManager:GetAvailableSlots()
    local slots = {}
    for i = 1, 10 do
        local slotName = "slot" .. tostring(i)
        if SaveData.DoesSaveExist(slotName) then
            table.insert(slots, i)
        end
    end
    return slots
end

-- Delete a save slot
function SaveSlotManager:DeleteSlot(slotNumber)
    local slotName = "slot" .. tostring(slotNumber)
    if SaveData.DoesSaveExist(slotName) then
        SaveData.DeleteSave(slotName)
        Log.Debug("Deleted save slot " .. tostring(slotNumber))
    end
end
```

## Settings / Preferences

SaveData is also useful for player preferences that persist across play sessions:

```lua
function SettingsManager:LoadSettings()
    if SaveData.DoesSaveExist("settings") then
        SaveData.Load("settings")
    end

    -- Read settings with defaults
    self.musicVolume = SaveData.GetFloat("musicVolume", 0.8)
    self.sfxVolume = SaveData.GetFloat("sfxVolume", 1.0)
    self.difficulty = SaveData.GetInt("difficulty", 1)
    self.invertY = SaveData.GetBool("invertY", false)
    self.hudColor = SaveData.GetColor("hudColor", Vec(1, 1, 1, 1))
end

function SettingsManager:SaveSettings()
    SaveData.SetFloat("musicVolume", self.musicVolume)
    SaveData.SetFloat("sfxVolume", self.sfxVolume)
    SaveData.SetInt("difficulty", self.difficulty)
    SaveData.SetBool("invertY", self.invertY)
    SaveData.SetColor("hudColor", self.hudColor)
    SaveData.Save("settings")
end
```

## Lua API Reference

See [SaveData Lua API](../Lua/Misc/SaveData.md) for the full function reference.

## Things to Know

- **In-memory store is global.** There is one shared key-value map. Calling `Load` replaces all current data with the contents of the save file.
- **Save and Load are separate.** Setting values only changes the in-memory data. You must call `SaveData.Save(name)` to write to disk.
- **Type-safe getters.** `GetInt` only returns a value if the stored type is Integer. If the key exists but holds a different type, the default is returned.
- **Default values.** All getters accept an optional second argument for the default value. If omitted, sensible defaults are used (0, 0.0, false, "", zero vectors).
- **GameCube memory card.** On GameCube, saves go to Memory Card Slot A via the CARD API. Call `System.UnmountMemoryCard()` when you're done if you need to release the card.
- **Save slot names are filenames.** On filesystem platforms, the slot name becomes the filename in the `Saves/` directory. Avoid special characters in slot names.
