# 对话脚本说明（dialogue_script.json）

配置文件：

```
assets/data/dialogue_script.json
```

## 1) 数据结构（极简）

`dialogue_script.json` 是一个“字典”：
- key：对话 id（string）
- value：字符串数组，每个元素是一行对话（按顺序展示）

示例：

```json
{
  "friend_intro": [
    "Hey there! Welcome to the valley.",
    "Let me know if you need anything."
  ]
}
```

## 2) 如何把 NPC 绑定到脚本

NPC 的 `DialogueComponent.dialogue_id_` 来自 actor blueprint 的 `dialogue_id` 字段（string）。

例如 `assets/data/actor_blueprint.json`：

```json
"friend": {
  "dialogue_id": "friend_intro"
}
```

运行时：
- `BlueprintManager` 会把 `dialogue_id` 字符串哈希成 `entt::id_type`
- `DialogueSystem` 加载 `dialogue_script.json` 时也会对 key 做同样的哈希

因此你只需要保证：
- blueprint 的 `dialogue_id` 字符串 **与** `dialogue_script.json` 的 key 完全一致

## 3) 行为与常见问题

- 按 `F` 触发交互：显示第 1 行
- 再按 `F`：推进到下一行；到末尾自动关闭
- key 不存在 / 数组为空：会打印 warning，并忽略这次对话触发

