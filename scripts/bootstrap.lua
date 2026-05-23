-- TinyFarm script bootstrap.
-- GameRuntimeAssembler loads this file every time GameScene is initialized,
-- including after loading a save. Keep top-level work idempotent; persistent
-- story state must go through tf.state once Phase 4 lands.

local event = tf.script.require("lib.event")
tf.script.require("lib.dialogue")
tf.script.require("lib.quest")
tf.script.require("lib.state")
tf.script.require("quests.first_delivery")
tf.script.require("npcs.lyria")

if event ~= nil then
    event.on_day_changed(function(evt)
        print("[tf] day_changed " .. tostring(evt.day))
    end)
end

print("[tf] bootstrap loaded")
