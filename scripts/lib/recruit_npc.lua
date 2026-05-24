local dialogue = tf.script.require("lib.dialogue")

local recruit_npc = {}

local function normalize_lines(value)
    if type(value) == "string" and value ~= "" then
        return {value}
    end
    if type(value) == "table" then
        return value
    end
    return nil
end

local function after_dialogue(action)
    return function(interrupted)
        if interrupted or action == nil then
            return
        end

        local result = action()
        if result ~= nil and result.ok ~= true then
            print("[tf] recruit action failed: " .. tostring(result.reason))
        end
    end
end

local function handle(evt, lines, action)
    if dialogue.start(evt.target, lines, after_dialogue(action)) then
        evt.dialogue_handled = true
    end
end

function recruit_npc.register(opts)
    if type(opts) ~= "table" or type(opts.actor_id) ~= "string" or opts.actor_id == "" then
        return false
    end

    local actor_id = opts.actor_id
    local intro_lines = normalize_lines(opts.intro_lines)
    local recruited_lines = normalize_lines(opts.recruited_lines or opts.recruited_line)
    if intro_lines == nil or recruited_lines == nil then
        return false
    end

    tf.event.on("interact", function(evt)
        -- lib.dialogue marks this when the same keypress advanced or closed a sequence.
        if evt.dialogue_handled or evt.target == nil or evt.target_actor_id ~= actor_id then
            return
        end

        if tf.party.is_recruited(actor_id) then
            handle(evt, recruited_lines)
            return
        end

        handle(evt, intro_lines, function()
            return tf.party.request_recruit(actor_id, evt.target)
        end)
    end)

    return true
end

return recruit_npc
