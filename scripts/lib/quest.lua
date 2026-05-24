local quest = {}

function quest.id(value)
    return value
end

function quest.module_for(quest_id)
    if type(quest_id) ~= "string" or quest_id == "" then
        return nil
    end

    local slug = quest_id
    if string.sub(slug, 1, 6) == "quest." then
        slug = string.sub(slug, 7)
    end
    if slug == "" or string.find(slug, "^%.") or string.find(slug, "%.$") or string.find(slug, "%.%.") then
        return nil
    end

    slug = string.gsub(slug, "%.", "_")
    if not string.match(slug, "^[%w_]+$") then
        return nil
    end

    return "quests." .. slug
end

return quest
