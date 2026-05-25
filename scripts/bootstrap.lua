-- TinyFarm script bootstrap.
-- GameRuntimeAssembler loads this file every time GameScene is initialized,
-- including after loading a save. Keep top-level work idempotent; persistent
-- story state must go through tf.state.

local event = tf.script.require("lib.event")
local quest = tf.script.require("lib.quest")
local state = tf.script.require("lib.state")
-- Load lib.dialogue before NPC modules so its global interact advancer runs
-- before NPC/quest scripts start new dialogue sequences.
tf.script.require("lib.dialogue")
tf.script.require("lib.once")
tf.script.require("maps.home_exterior")
tf.script.require("quests.first_delivery")
tf.script.require(quest.module_for("quest.village.goblin_cleanup"))
tf.script.require("npcs.greeter")
tf.script.require("npcs.lyria")
tf.script.require("npcs.tori")
tf.script.require("npcs.merchant")

local initial_gold_key = "player.initial_gold_300_seeded"
if state.get_bool(initial_gold_key, false) ~= true and tf.player.exists() then
    if tf.player.set_gold(300) then
        state.set(initial_gold_key, true)
    end
end

if event ~= nil then
    event.on_day_changed(function(evt)
        print("[tf] day_changed " .. tostring(evt.day))
    end)
end

print("[tf] bootstrap loaded")
