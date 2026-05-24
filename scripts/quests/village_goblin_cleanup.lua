local dialogue = tf.script.require("lib.dialogue")
local state = tf.script.require("lib.state")

local quest = {
    id = "quest.village.goblin_cleanup",
    giver_actor_id = "npc.manu",
    objective_id = "kill_slimes",
}

local function is_giver(evt)
    return evt.target ~= nil and evt.target_actor_id == quest.giver_actor_id
end

local function after_dialogue(action)
    return function(interrupted)
        if interrupted or action == nil then
            return
        end

        local result = action()
        if result ~= nil and result.ok ~= true then
            print("[tf] quest action failed: " .. tostring(result.reason))
        end
    end
end

local function handle(evt, lines, action)
    if dialogue.start(evt.target, lines, after_dialogue(action)) then
        evt.dialogue_handled = true
    end
end

tf.event.on("interact", function(evt)
    -- lib.dialogue marks this when the same keypress advanced or closed a sequence.
    if evt.dialogue_handled or not is_giver(evt) then
        return
    end

    local status = tf.quest.status(quest.id)
    if status == "offerable" and tf.quest.is_available(quest.id) then
        handle(evt, {
            "Can you help us drive away the slimes?",
            "The old path is crawling with them. Three should be enough.",
        }, function()
            return tf.quest.offer(quest.id, evt.target)
        end)
    elseif status == "in_progress" then
        local progress = tf.quest.progress(quest.id, quest.objective_id)
        handle(evt, {
            "Still seeing slimes out there. " .. progress.current .. "/" .. progress.required .. " cleared.",
        })
    elseif status == "ready_to_turn_in" then
        handle(evt, {
            "You did it? That's a relief.",
            "Here, take this for the trouble.",
        }, function()
            return tf.quest.turn_in(quest.id, evt.target)
        end)
    elseif status == "completed" then
        handle(evt, {"Thank you again. The village path feels safe now."})
    end
end)

local function mark(name, evt)
    if evt.quest_id == quest.id then
        state.set("quest.village_goblin_cleanup." .. name, true)
        state.add("quest.village_goblin_cleanup." .. name .. "_count", 1)
    end
end

tf.event.on("quest_accepted", function(evt) mark("accepted", evt) end)
tf.event.on("quest_completed", function(evt) mark("completed", evt) end)
return quest
