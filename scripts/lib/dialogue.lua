local dialogue = {}

local CHANNEL_CONVERSATION = tf.dialogue.CHANNEL_CONVERSATION
local active = {}
local choice_callbacks = {}
local current_key = nil

local function handle_key(target)
    if target == nil or target.entity_id == nil then
        return nil
    end
    return tostring(target.entity_id)
end

local function copy_lines(lines)
    if type(lines) ~= "table" then
        return nil
    end

    local copied = {}
    for index, line in ipairs(lines) do
        if type(line) == "string" and line ~= "" then
            copied[#copied + 1] = line
        end
    end
    return copied
end

local function start_args(opts_or_done, maybe_done)
    if type(opts_or_done) == "table" then
        return opts_or_done, maybe_done
    end
    return {}, opts_or_done
end

local function resolve_speaker(target, speaker, speaker_actor_id, channel)
    if channel == CHANNEL_CONVERSATION and target ~= nil then
        if speaker == nil then
            speaker = tf.entity.name(target)
        end
        if speaker_actor_id == nil then
            speaker_actor_id = tf.entity.actor_id(target)
        end
    end
    return speaker or "", speaker_actor_id or ""
end

local function show_line(state)
    return tf.dialogue.show(
        state.lines[state.cursor],
        state.speaker,
        CHANNEL_CONVERSATION,
        state.target,
        state.speaker_actor_id)
end

local function complete(key, interrupted, emit_hide)
    local state = active[key]
    if state == nil then
        return false
    end

    active[key] = nil
    if current_key == key then
        current_key = nil
    end

    if emit_hide ~= false then
        tf.dialogue.hide(CHANNEL_CONVERSATION, state.target)
    end
    if type(state.on_done) == "function" then
        state.on_done(interrupted == true)
    end
    return true
end

local function advance(target)
    local key = handle_key(target)
    if key == nil then
        return false
    end

    if current_key ~= nil and current_key ~= key then
        complete(current_key, true)
        return false
    end

    local state = active[key]
    if state == nil then
        return false
    end

    state.cursor = state.cursor + 1
    if state.cursor <= #state.lines then
        return show_line(state)
    end

    return complete(key, false)
end

function dialogue.show(text, speaker, channel, target, speaker_actor_id)
    speaker, speaker_actor_id = resolve_speaker(target, speaker, speaker_actor_id, channel)
    return tf.dialogue.show(text, speaker, channel, target, speaker_actor_id)
end

function dialogue.hide(channel, target)
    return tf.dialogue.hide(channel, target)
end

function dialogue.start(target, lines, opts_or_done, maybe_done)
    local key = handle_key(target)
    local copied_lines = copy_lines(lines)
    if key == nil or copied_lines == nil or #copied_lines == 0 then
        return false
    end

    local opts, on_done = start_args(opts_or_done, maybe_done)
    local speaker, speaker_actor_id = resolve_speaker(
        target,
        opts.speaker,
        opts.speaker_actor_id,
        CHANNEL_CONVERSATION)

    if current_key ~= nil and current_key ~= key then
        complete(current_key, true)
    end
    if active[key] ~= nil then
        complete(key, true)
    end

    active[key] = {
        target = target,
        lines = copied_lines,
        cursor = 1,
        speaker = speaker,
        speaker_actor_id = speaker_actor_id,
        on_done = on_done,
    }
    current_key = key
    return show_line(active[key])
end

function dialogue.cancel(target)
    local key = handle_key(target)
    if key == nil then
        key = current_key
    end
    if key == nil then
        return false
    end
    return complete(key, true)
end

function dialogue.choice(target, prompt, choices, opts_or_done, maybe_done)
    local opts, on_done = start_args(opts_or_done, maybe_done)
    local speaker, speaker_actor_id = resolve_speaker(
        target,
        opts.speaker,
        opts.speaker_actor_id,
        CHANNEL_CONVERSATION)

    local request_id = tf.dialogue.choice(prompt, choices, {
        target = target,
        speaker = speaker,
        speaker_actor_id = speaker_actor_id,
        allow_cancel = opts.allow_cancel ~= false,
    })
    if request_id == nil or request_id == 0 then
        return false
    end

    if type(on_done) == "function" then
        choice_callbacks[tostring(request_id)] = on_done
    end
    return true
end

tf.event.on("interact", function(evt)
    if advance(evt.target) then
        evt.dialogue_handled = true
    end
end)

tf.event.on("dialogue_closed", function(evt)
    if evt.channel ~= CHANNEL_CONVERSATION then
        return
    end

    local key = handle_key(evt.target)
    if key == nil then
        key = current_key
    end
    if key == nil then
        return
    end

    complete(key, true, false)
end)

tf.event.on("dialogue_choice_selected", function(evt)
    local callback = choice_callbacks[tostring(evt.request_id)]
    if callback == nil then
        return
    end

    choice_callbacks[tostring(evt.request_id)] = nil
    callback({
        request_id = evt.request_id,
        target = evt.target,
        cancelled = evt.cancelled == true,
        index = evt.choice_index,
        zero_index = evt.choice_zero_index,
        id = evt.choice_id,
        label = evt.choice_label,
    })
end)

return dialogue
