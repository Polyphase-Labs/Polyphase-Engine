--- @meta

---@class SaveDataModule
SaveData = {}

---@param key string
---@param value integer
function SaveData.SetInt(key, value) end

---@param key string
---@return integer
function SaveData.GetInt(key) end

---@param key string
---@param value number
function SaveData.SetFloat(key, value) end

---@param key string
---@return number
function SaveData.GetFloat(key) end

---@param key string
---@param value boolean
function SaveData.SetBool(key, value) end

---@param key string
---@return boolean
function SaveData.GetBool(key) end

---@param key string
---@param value string
function SaveData.SetString(key, value) end

---@param key string
---@return string
function SaveData.GetString(key) end

---@param key string
---@param vec Vector
function SaveData.SetVector(key, vec) end

---@param key string
---@param defaultValue Vector
---@return Vector
function SaveData.GetVector(key, defaultValue) end

---@param key string
---@param vec Vector
function SaveData.SetVector2D(key, vec) end

---@param key string
---@param defaultValue Vector
---@return Vector
function SaveData.GetVector2D(key, defaultValue) end

---@param key string
---@param value Vector
function SaveData.SetColor(key, value) end

---@param key string
---@param defaultValue Vector
---@return Vector
function SaveData.GetColor(key, defaultValue) end

---@param saveName string
function SaveData.Save(saveName) end

---@param saveName string
function SaveData.Load(saveName) end

---@param saveName string
---@return boolean
function SaveData.DoesSaveExist(saveName) end

---@param saveName string
function SaveData.DeleteSave(saveName) end

---@param key string
---@return boolean
function SaveData.HasKey(key) end

---@param key string
function SaveData.DeleteKey(key) end

function SaveData.DeleteAll() end
