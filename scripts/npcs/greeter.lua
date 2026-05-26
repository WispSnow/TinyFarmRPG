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
    if dialogue.start(evt.target, {
        tf.i18n.tr("dialogue.greeter.hi"),
        tf.i18n.tr("dialogue.greeter.bye"),
    }, function()
        active = false
    end) then
        evt.dialogue_handled = true
    else
        active = false
    end
end)

return greeter
