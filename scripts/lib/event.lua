local event = {}

function event.on(name, fn)
    return tf.event.on(name, fn)
end

function event.on_interact(fn)
    return tf.callbacks.on_interact(fn)
end

function event.on_dialogue_closed(fn)
    return tf.callbacks.on_dialogue_closed(fn)
end

function event.on_dialogue_choice_selected(fn)
    return tf.callbacks.on_dialogue_choice_selected(fn)
end

function event.on_battle_start(fn)
    return tf.callbacks.on_battle_start(fn)
end

function event.on_battle_end(fn)
    return tf.callbacks.on_battle_end(fn)
end

function event.on_day_changed(fn)
    return tf.callbacks.on_day_changed(fn)
end

function event.on_time_of_day_changed(fn)
    return tf.callbacks.on_time_of_day_changed(fn)
end

function event.on_inventory_changed(fn)
    return tf.callbacks.on_inventory_changed(fn)
end

function event.on_item_used(fn)
    return tf.callbacks.on_item_used(fn)
end

function event.on_quest_accepted(fn)
    return tf.callbacks.on_quest_accepted(fn)
end

function event.on_quest_completed(fn)
    return tf.callbacks.on_quest_completed(fn)
end

return event
