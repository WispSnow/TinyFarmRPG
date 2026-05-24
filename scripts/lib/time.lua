local time = {}

local DAY_START_HOUR = 6
local NIGHT_START_HOUR = 18

local function normalize_hour(hour)
    if type(hour) == "number" then
        return hour
    end
    return tf.time.hour()
end

function time.is_night_hour(hour)
    local value = normalize_hour(hour)
    return value < DAY_START_HOUR or value >= NIGHT_START_HOUR
end

function time.is_day_hour(hour)
    return not time.is_night_hour(hour)
end

function time.is_night()
    return time.is_night_hour(tf.time.hour())
end

function time.is_day()
    return not time.is_night()
end

return time
