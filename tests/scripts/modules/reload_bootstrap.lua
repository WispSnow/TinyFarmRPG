local counter = tf.script.require("lib.reload_counter")

tf.callbacks.on_interact(function()
    reload_callback_hits = (reload_callback_hits or 0) + 1
    counter.hits = counter.hits + 1
end)
