--[[
    VR Recoil Configuration (RE7 / RE8 / RE2 / RE3)
    -------------------------------------------------
    This script manages per-weapon recoil intensity persistence for REFramework VR recoil.
    - Load: On startup, calls load_recoil_settings() so C++ loads recoil_settings.json
      from reframework/data (inside the game folder).
    - Save: When the user changes intensity in the REFramework menu (C++) or in this script's
      UI, the C++ side writes recoil_settings.json to reframework/data. This script can call
      save_recoil_settings() after updating a value via set_per_weapon_recoil_intensity().

    Weapon ID mapping (game-specific):
    RE8: weapon ID = owner GameObject name (e.g. "ri3042_Inventory"). Weapon type is always WeaponGunCore.
    RE7: weapon ID = weapon's GameObject name from get_GameObject() (app.WeaponGun). Naming may differ from RE8.
    RE2/RE3: weapon ID = equipped gun's GameObject name (implement.Gun get_GameObject). Uses firstpersonmod.
    New weapons get default intensity 1.0 until the user sets a value and saves.
]]

local game_name = reframework and reframework.get_game_name and reframework.get_game_name()
local is_re7 = game_name == "re7"
local is_re8 = game_name == "re8"
local is_re2 = game_name == "re2"
local is_re3 = game_name == "re3"

if not is_re7 and not is_re8 and not is_re2 and not is_re3 then
    return
end

-- RE7/RE8: re8vr; RE2/RE3: firstpersonmod (FirstPerson mod exposes same recoil API).
local function get_recoil_api()
    if is_re7 or is_re8 then
        if re8vr == nil or type(re8vr.load_recoil_settings) ~= "function" then
            return nil
        end
        return re8vr
    end
    if is_re2 or is_re3 then
        if firstpersonmod == nil or type(firstpersonmod.load_recoil_settings) ~= "function" then
            return nil
        end
        return firstpersonmod
    end
    return nil
end

-- Load recoil settings from recoil_settings.json into C++ on startup.
local function init()
    local r = get_recoil_api()
    if r then
        r:load_recoil_settings()
    end
end

init()

-- Optional: draw a small UI section for per-weapon recoil when REFramework menu is open.
local function draw_recoil_ui()
    local r = get_recoil_api()
    if not r then
        return
    end

    if not imgui.tree_node("VR Recoil (Lua)") then
        return
    end

    imgui.indent()

    local current_id = r:get_current_weapon_recoil_id()
    if current_id == nil or current_id == "" then
        imgui.text("Current weapon: (none equipped)")
    else
        imgui.text("Current weapon: " .. tostring(current_id))
        local intensity = r:get_per_weapon_recoil_intensity(current_id)
        local changed, new_val = imgui.slider_float("Weapon recoil intensity (1-4)", intensity, 1.0, 4.0, "%.2f")
        if changed then
            r:set_per_weapon_recoil_intensity(current_id, new_val)
        end
        if imgui.button("Save recoil settings") then
            r:save_recoil_settings()
        end
    end

    imgui.unindent()
    imgui.tree_pop()
end

re.on_draw_ui(function()
    local r = get_recoil_api()
    if not r then
        return
    end
    local ok, err = pcall(draw_recoil_ui)
    if not ok and err then
        log.warn("[recoil_config] " .. tostring(err))
    end
end)
