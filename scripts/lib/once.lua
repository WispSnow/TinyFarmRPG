local state = tf.script.require("lib.state")

local once = {}

function once.is_done(key)
    return type(key) == "string" and key ~= "" and state.get_bool(key, false) == true
end

-- At-most-once helper: marks the key before running fn, so failures do not retry.
function once.run(key, fn)
    if type(key) ~= "string" or key == "" then
        return false
    end

    if once.is_done(key) then
        return false
    end

    if state.set(key, true) ~= true then
        return false
    end

    if type(fn) == "function" then
        fn()
    end
    return true
end

return once
