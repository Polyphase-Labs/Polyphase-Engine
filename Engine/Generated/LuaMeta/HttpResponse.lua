--- @meta

---@class HttpResponse
HttpResponse = {}

---@return boolean
function HttpResponse:IsSuccess() end

---@return integer
function HttpResponse:GetStatus() end

---@return string
function HttpResponse:GetError() end

---@return any
function HttpResponse:GetBody() end

---@param name string
---@return string
function HttpResponse:GetHeader(name) end

---@return any
function HttpResponse:GetHeaders() end

---@return string
function HttpResponse:GetFinalUrl() end

---@return nil, string
function HttpResponse:GetJson() end

---@return Asset
function HttpResponse:GetTexture() end

---@return Asset
function HttpResponse:GetSoundWave() end
