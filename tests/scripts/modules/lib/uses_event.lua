local event = tf.script.require("lib.event")

return {
    event_loaded = event ~= nil and event.loaded == true,
}
