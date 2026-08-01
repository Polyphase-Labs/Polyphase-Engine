--- @meta

---@class HttpModule
Http = {}

---@param url string
---@return any
function Http.Get(url) end

---@param url string
---@param body string
---@return any
function Http.Post(url, body) end

---@param url string
---@param body string
---@return any
function Http.Put(url, body) end

---@param url string
---@param body string
---@return any
function Http.Patch(url, body) end

---@param url string
---@return any
function Http.Delete(url) end

---@param url string
---@return any
function Http.Request(url) end

---@return boolean
function Http.IsAvailable() end

---@return string
function Http.GetMissingDependencyMessage() end
