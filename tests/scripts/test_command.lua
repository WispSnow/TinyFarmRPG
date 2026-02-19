function issue_add_item(item_id, count, target_id)
    if target_id == nil then
        return tf.command.add_item(item_id, count)
    end
    return tf.command.add_item(item_id, count, target_id)
end
