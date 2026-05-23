local state = {}

function state.available()
    return tf.state ~= nil
end

return state
