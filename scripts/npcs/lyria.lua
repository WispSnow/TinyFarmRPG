local event = tf.script.require("lib.event")

local lyria = {
    actor_id = "actor.lyria",
}

if event ~= nil then
    event.on_interact(function(evt)
        if evt.target_actor_id == lyria.actor_id or evt.target_name == "Lyria" then
            print("[tf] interacted with Lyria")
        end
    end)
end

return lyria
