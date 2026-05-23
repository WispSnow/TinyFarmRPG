local dialogue = {}

function dialogue.show(text, speaker, channel, target, speaker_actor_id)
    return tf.dialogue.show(text, speaker, channel, target, speaker_actor_id)
end

function dialogue.hide(channel, target)
    return tf.dialogue.hide(channel, target)
end

return dialogue
