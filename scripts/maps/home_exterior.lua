local dialogue = tf.script.require("lib.dialogue")
local once = tf.script.require("lib.once")

local map = {
    id = "home_exterior",
    first_enter_key = "map.home_exterior.first_enter",
    seed_cache_event = "home_exterior.seed_cache",
}

local function is_home_exterior(evt)
    return evt ~= nil and evt.map_id == map.id
end

local function show_first_enter_hint()
    dialogue.show(
        "The farm road is quiet today. Check the seed cache before heading out.",
        nil,
        tf.dialogue.CHANNEL_NOTICE)
end

local function on_map_enter(evt)
    if not is_home_exterior(evt) then
        return
    end

    once.run(map.first_enter_key, show_first_enter_hint)
end

local function open_seed_cache(evt)
    local once_key = evt.target_script_once_key or "map.home_exterior.seed_cache.opened"
    if once.run(once_key, function()
        dialogue.show("You found a few starter seeds.", nil, tf.dialogue.CHANNEL_ITEM_NOTICE, evt.target)
        tf.command.add_item("potato_seed", 2)
        tf.command.add_item("strawberry_seed", 3)
    end) then
        return
    end

    dialogue.show("The seed cache is empty.", nil, tf.dialogue.CHANNEL_NOTICE, evt.target)
end

local function on_interact(evt)
    if evt.dialogue_handled or evt.target == nil then
        return
    end
    if evt.target_kind ~= "chest" or evt.target_script_event ~= map.seed_cache_event then
        return
    end

    open_seed_cache(evt)
    evt.dialogue_handled = true
end

tf.event.on("map_enter", on_map_enter)
tf.event.on("interact", on_interact)

on_map_enter({map_id = tf.map.current()})

return map
