--- @meta

---@class HttpRequest
HttpRequest = {}

---@param k string
---@param v string
---@return any
function HttpRequest:Header(k, v) end

---@param body string
---@return any
function HttpRequest:Body(body) end

---@return any
function HttpRequest:Timeout() end

---@return any
function HttpRequest:VerifySsl() end

---@return any
function HttpRequest:Send() end
