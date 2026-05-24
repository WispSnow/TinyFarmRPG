local recruit_npc = tf.script.require("lib.recruit_npc")

local lyria = {
    actor_id = "actor.lyria",
}

assert(recruit_npc.register({
    actor_id = lyria.actor_id,
    intro_lines = {
        "Hey there! Welcome to the valley.",
        "I can help if you are heading into danger.",
    },
    recruited_line = "Stay safe out there. I'll be close by.",
}))

return lyria
