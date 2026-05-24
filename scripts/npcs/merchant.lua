local dialogue = tf.script.require("lib.dialogue")
local time = tf.script.require("lib.time")

local merchant = {
    actor_id = "npc.josh",
    quest_id = "quest.village.goblin_cleanup",
    day_shop_id = "shop.village.general.day",
    night_shop_id = "shop.village.general.night",
    post_quest_shop_id = "shop.village.general.post_slime_cleanup",
}

local function choose_shop()
    if tf.quest.status(merchant.quest_id) == "completed" then
        return merchant.post_quest_shop_id, {
            "Welcome back. I've set aside better stock.",
        }
    end

    if time.is_night() then
        return merchant.night_shop_id, {
            "Welcome. Let me open the night shelf.",
        }
    end

    return merchant.day_shop_id, {
        "Welcome. Let me open the day shelf.",
    }
end

tf.event.on("interact", function(evt)
    if evt.dialogue_handled or evt.target == nil or evt.target_actor_id ~= merchant.actor_id then
        return
    end

    local shop_id, lines = choose_shop()
    if dialogue.start(evt.target, lines, function(interrupted)
        if interrupted then
            return
        end

        local result = tf.shop.open(shop_id, evt.target)
        if result ~= nil and result.ok ~= true then
            print("[tf] shop open failed: " .. tostring(result.reason))
            dialogue.show("Sorry, the shop is closed.", nil, tf.dialogue.CHANNEL_CONVERSATION, evt.target)
        end
    end) then
        evt.dialogue_handled = true
    end
end)

return merchant
