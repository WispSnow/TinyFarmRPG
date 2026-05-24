-- TinyFarm script bootstrap.
-- GameRuntimeAssembler loads this file every time GameScene is initialized,
-- including after loading a save. Keep top-level work idempotent; persistent
-- story state must go through tf.state.

local event = tf.script.require("lib.event")
local quest = tf.script.require("lib.quest")
-- Load lib.dialogue before NPC modules so its global interact advancer runs
-- before NPC/quest scripts start new dialogue sequences.
tf.script.require("lib.dialogue")
tf.script.require("lib.state")
tf.script.require("quests.first_delivery")
tf.script.require(quest.module_for("quest.village.goblin_cleanup"))
tf.script.require("npcs.greeter")
tf.script.require("npcs.lyria")

if event ~= nil then
    event.on_day_changed(function(evt)
        print("[tf] day_changed " .. tostring(evt.day))
    end)
end

print("[tf] bootstrap loaded")
