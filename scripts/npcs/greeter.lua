local dialogue = tf.script.require("lib.dialogue")

local greeter = {
    actor_id = "npc.greeter",
}

local active = false

tf.event.on("interact", function(evt)
    if evt.dialogue_handled then
        return
    end
    if evt.target_actor_id ~= greeter.actor_id and evt.target_name ~= "Greeter" then
        return
    end
    if active then
        return
    end

    active = true
    dialogue.start(evt.target, {"Hi", "Bye"}, function()
        active = false
    end)
end)

return greeter
