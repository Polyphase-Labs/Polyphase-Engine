# SaveData

A key-value save system for persisting game data across sessions. Data is stored in memory and can be saved to / loaded from named save slots. Supports integers, floats, booleans, strings, vectors, and colors.

Save files are written through the platform save system (`System.WriteSave` internally), so they automatically go to the correct location on each platform (filesystem on PC, memory card on GameCube, etc.).

---
### SetInt
Set an integer value.

Sig: `SaveData.SetInt(key, value)`
 - Arg: `string key` Key name
 - Arg: `integer value` Integer value
---
### GetInt
Get an integer value, or a default if the key is missing.

Sig: `value = SaveData.GetInt(key, default=0)`
 - Arg: `string key` Key name
 - Arg: `integer default` Default value if key not found
 - Ret: `integer value` Stored or default value
---
### SetFloat
Set a float value.

Sig: `SaveData.SetFloat(key, value)`
 - Arg: `string key` Key name
 - Arg: `number value` Float value
---
### GetFloat
Get a float value, or a default if the key is missing.

Sig: `value = SaveData.GetFloat(key, default=0.0)`
 - Arg: `string key` Key name
 - Arg: `number default` Default value if key not found
 - Ret: `number value` Stored or default value
---
### SetBool
Set a boolean value.

Sig: `SaveData.SetBool(key, value)`
 - Arg: `string key` Key name
 - Arg: `boolean value` Boolean value
---
### GetBool
Get a boolean value, or a default if the key is missing.

Sig: `value = SaveData.GetBool(key, default=false)`
 - Arg: `string key` Key name
 - Arg: `boolean default` Default value if key not found
 - Ret: `boolean value` Stored or default value
---
### SetString
Set a string value.

Sig: `SaveData.SetString(key, value)`
 - Arg: `string key` Key name
 - Arg: `string value` String value
---
### GetString
Get a string value, or a default if the key is missing.

Sig: `value = SaveData.GetString(key, default="")`
 - Arg: `string key` Key name
 - Arg: `string default` Default value if key not found
 - Ret: `string value` Stored or default value
---
### SetVector
Set a 3D vector value.

Sig: `SaveData.SetVector(key, value)`
 - Arg: `string key` Key name
 - Arg: `Vector value` 3-component vector
---
### GetVector
Get a 3D vector value, or a default if the key is missing.

Sig: `value = SaveData.GetVector(key, default=Vec(0,0,0))`
 - Arg: `string key` Key name
 - Arg: `Vector default` Default value if key not found
 - Ret: `Vector value` Stored or default value
---
### SetVector2D
Set a 2D vector value.

Sig: `SaveData.SetVector2D(key, value)`
 - Arg: `string key` Key name
 - Arg: `Vector value` 2-component vector
---
### GetVector2D
Get a 2D vector value, or a default if the key is missing.

Sig: `value = SaveData.GetVector2D(key, default=Vec(0,0))`
 - Arg: `string key` Key name
 - Arg: `Vector default` Default value if key not found
 - Ret: `Vector value` Stored or default value
---
### SetColor
Set a color (4-component vector) value.

Sig: `SaveData.SetColor(key, value)`
 - Arg: `string key` Key name
 - Arg: `Vector value` 4-component color vector (RGBA)
---
### GetColor
Get a color value, or a default if the key is missing.

Sig: `value = SaveData.GetColor(key, default=Vec(0,0,0,0))`
 - Arg: `string key` Key name
 - Arg: `Vector default` Default value if key not found
 - Ret: `Vector value` Stored or default value
---
### Save
Write all in-memory data to a named save slot.

Sig: `SaveData.Save(name)`
 - Arg: `string name` Save slot name (used as filename)
---
### Load
Load data from a named save slot into memory. Clears any existing in-memory data first. Logs a warning if the save does not exist.

Sig: `SaveData.Load(name)`
 - Arg: `string name` Save slot name
---
### DoesSaveExist
Check if a save slot file exists on disk.

Sig: `exists = SaveData.DoesSaveExist(name)`
 - Arg: `string name` Save slot name
 - Ret: `boolean exists` Whether the save file exists
---
### DeleteSave
Delete a save slot file from disk.

Sig: `SaveData.DeleteSave(name)`
 - Arg: `string name` Save slot name
---
### HasKey
Check if a key exists in the current in-memory data.

Sig: `exists = SaveData.HasKey(key)`
 - Arg: `string key` Key name
 - Ret: `boolean exists` Whether the key exists
---
### DeleteKey
Remove a key from the in-memory data. Does not affect the save file until `Save` is called.

Sig: `SaveData.DeleteKey(key)`
 - Arg: `string key` Key name
---
### DeleteAll
Clear all in-memory data. Does not affect save files on disk.

Sig: `SaveData.DeleteAll()`
---
