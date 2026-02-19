function issue_add_item(item_id, count, target_handle)
    if target_handle == nil then
        return tf.command.add_item(item_id, count)
    end
    return tf.command.add_item(item_id, count, target_handle)
end
