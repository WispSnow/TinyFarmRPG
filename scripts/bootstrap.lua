-- TinyFarm scripting bootstrap
print("Script bootstrap loaded")
if tf and tf.time and tf.dialogue then
    local msg = string.format(
        "Script bootstrap loaded at %s",
        tf.time.formatted()
    )
    tf.dialogue.show(msg, "Script", 1)
end
