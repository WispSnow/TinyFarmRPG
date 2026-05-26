local recruit_npc = tf.script.require("lib.recruit_npc")

local tori = {
    actor_id = "actor.tori",
}

assert(recruit_npc.register({
    actor_id = tori.actor_id,
    intro_lines = function()
        return {
            tf.i18n.tr("dialogue.tori.intro.1"),
            tf.i18n.tr("dialogue.tori.intro.2"),
        }
    end,
    recruited_line = function()
        return tf.i18n.tr("dialogue.tori.recruited")
    end,
}))

return tori
