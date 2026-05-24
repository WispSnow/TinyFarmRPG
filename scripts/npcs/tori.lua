local recruit_npc = tf.script.require("lib.recruit_npc")

local tori = {
    actor_id = "actor.tori",
}

assert(recruit_npc.register({
    actor_id = tori.actor_id,
    intro_lines = {
        "The road out there looks rough.",
        "Let me join you. I can hold my own.",
    },
    recruited_line = "You know where to find me when the road gets rough.",
}))

return tori
