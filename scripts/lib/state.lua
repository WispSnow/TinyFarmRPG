local state = {}

function state.available()
    return tf.state ~= nil
end

function state.has(key)
    return tf.state.has(key)
end

function state.get(key, default)
    return tf.state.get(key, default)
end

function state.get_number(key, default)
    return tf.state.get_number(key, default)
end

function state.get_int(key, default)
    return tf.state.get_int(key, default)
end

function state.get_bool(key, default)
    return tf.state.get_bool(key, default)
end

function state.get_string(key, default)
    return tf.state.get_string(key, default)
end

function state.set(key, value)
    return tf.state.set(key, value)
end

function state.add(key, amount)
    return tf.state.add(key, amount)
end

function state.unset(key)
    return tf.state.unset(key)
end

return state
