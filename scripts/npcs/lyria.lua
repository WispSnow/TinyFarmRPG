local recruit_npc = tf.script.require("lib.recruit_npc")

local lyria = {
    actor_id = "actor.lyria",
}

assert(recruit_npc.register({
    actor_id = lyria.actor_id,
    intro_lines = function()
        return {
            tf.i18n.tr("dialogue.lyria.intro.1"),
            tf.i18n.tr("dialogue.lyria.intro.2"),
        }
    end,
    recruited_line = function()
        return tf.i18n.tr("dialogue.lyria.recruited")
    end,
}))

return lyria
