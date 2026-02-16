# 光照配置说明 (Light Configuration Guide)

本文档说明如何通过 `light_config.json` 调整游戏的昼夜光照效果。

## 配置文件位置

```
assets/data/light_config.json
```

## 配置结构

### 1. 过渡时段 (transition_periods)

控制日出和日落的时间范围。

```json
"transition_periods": {
    "sunrise": {
        "start": 5.0,   // 日出开始时间（小时）
        "end": 7.0      // 日出结束时间（小时）
    },
    "sunset": {
        "start": 17.0,  // 日落开始时间（小时）
        "end": 19.0     // 日落结束时间（小时）
    }
}
```

**效果说明：**
- `sunrise`: 太阳从地平线下升起，月亮落下的过渡期
- `sunset`: 太阳落下，月亮升起的过渡期
- 时间跨度越大 = 过渡越慢越柔和
- 时间跨度越小 = 过渡越快

**推荐值：**
- 快速过渡：1-1.5 小时（例如 5.0-6.5）
- 标准过渡：2 小时（例如 5.0-7.0）
- 缓慢过渡：3-4 小时（例如 5.0-8.0 或 9.0）

### 2. 太阳配置 (sun)

控制白天太阳光的特性。

```json
"sun": {
    "color": {
        "warmth_variation": 0.2  // 色温变化幅度（0.0-1.0）
    },
    "intensity": {
        "base_min": 0.4,  // 最小强度（日出/日落时）
        "base_max": 0.8   // 最大强度增量（正午时）
    },
    "offset": {
        "min": 0.1,  // 最小偏移值
        "max": 0.2   // 最大偏移增量
    },
    "softness": 0.22  // 光照柔和度（0.0-0.5）
}
```

**参数说明：**
- `warmth_variation`: 控制早晚橙色调的强度
  - 0.0 = 全天保持冷白色
  - 0.2 = 标准（早晚暖橙，正午冷白）
  - 0.5+ = 强烈的橙色调

- `intensity.base_min`: 日出/日落时的基础亮度
- `intensity.base_max`: 正午时的额外亮度
- 实际强度 = base_min + base_max × sin(时间因子)

- `softness`: 光照边缘的柔和程度
  - 0.1 = 硬边缘，对比强烈
  - 0.25 = 标准柔和
  - 0.4+ = 非常柔和，对比弱

### 3. 月亮配置 (moon)

控制夜晚月光的特性。

```json
"moon": {
    "color": {
        "r": 0.65,  // 红色分量（0.0-1.0）
        "g": 0.7,   // 绿色分量（0.0-1.0）
        "b": 0.9    // 蓝色分量（0.0-1.0）
    },
    "intensity": 0.2,  // 月光强度
    "offset": 0.25,    // 光照偏移
    "softness": 0.38   // 光照柔和度
}
```

**效果建议：**
- **标准冷蓝月光**: `{r:0.65, g:0.7, b:0.9}`
- **温暖月光**: `{r:0.8, g:0.8, b:0.85}`
- **神秘紫月**: `{r:0.7, g:0.6, b:0.95}`
- **明亮月光**: intensity = 0.3-0.4
- **昏暗月光**: intensity = 0.1-0.15

### 4. 环境光关键帧 (ambient.keyframes)

定义全天不同时段的基础环境光颜色。

```json
"ambient": {
    "keyframes": [
        {
            "hour": 4.0,
            "color": {"r": 0.08, "g": 0.08, "b": 0.12},
            "comment": "深夜末尾 - 深蓝"
        },
        {
            "hour": 6.0,
            "color": {"r": 0.15, "g": 0.13, "b": 0.12},
            "comment": "黎明 - 暗橙"
        },
        // ... 更多关键帧
    ]
}
```

**关键帧规则：**
- 必须按时间顺序排列（hour 从小到大）
- 第一个关键帧的 hour 应 < 最后一个关键帧
- 系统会在关键帧之间自动插值
- 可以添加或删除关键帧来改变一天的光照节奏

**颜色值建议：**
- **深夜**: `{r:0.08, g:0.08, b:0.12}` - 深蓝灰
- **黎明**: `{r:0.15, g:0.13, b:0.12}` - 暗橙灰
- **早晨**: `{r:0.28, g:0.28, b:0.26}` - 中性灰
- **正午**: `{r:0.35, g:0.35, b:0.35}` - 明亮灰白
- **黄昏**: `{r:0.22, g:0.20, b:0.18}` - 暖灰
- **夜晚**: `{r:0.08, g:0.08, b:0.12}` - 深蓝灰

### 5. 自发光显隐 (emissive_visibility.dark_time)

用于路灯/窗户等“自发光光源”的显隐时间窗口（仅控制带 `NightOnlyTag/DayOnlyTag` 的光源实体）。

```json
"emissive_visibility": {
    "dark_time": {
        "start": 18.0,
        "end": 6.0
    }
}
```

**规则说明：**
- `dark_time.start/end` 为小时（float），支持跨天：当 `start > end` 时表示窗口跨过午夜（例如 18.0 → 6.0）
- 这是**视觉规则**，与玩法中的 `TimeOfDay::Night`（由 `game_time_config.json` 定义）是不同概念
- 该窗口用于：
  - 地图加载时：设置初始 `LightDisabledTag`（只影响灯光/自发光，不隐藏精灵）
  - 时间推进时：在 `HourChangedEvent` 上切换 `LightDisabledTag`

### 6. 玩家跟随点光源 (player_follow_light)

用于给玩家实体挂载一个可切换的点光源，并支持通过 offset 调整“略高于脚下”的位置。

```json
"player_follow_light": {
    "enabled_by_default": false,
    "radius": 128.0,
    "intensity": 1.0,
    "color": { "r": 1.0, "g": 0.95, "b": 0.8 },
    "offset": { "x": 0.0, "y": -10.0 }
}
```

**规则说明：**
- 室外（`in_world=true`）：仅在 `emissive_visibility.dark_time` 时间窗口内允许点亮
- 室内（`in_world=false`）：任意时间允许点亮
- offset 为世界像素偏移，y 轴向下为正

## 调整示例

### 示例 1: 快速日夜交替

适合加速测试或快节奏游戏。

```json
"transition_periods": {
    "sunrise": {"start": 5.5, "end": 6.5},
    "sunset": {"start": 17.5, "end": 18.5}
}
```

### 示例 2: 明亮的白天

增强白天亮度。

```json
"sun": {
    "intensity": {
        "base_min": 0.6,
        "base_max": 1.0
    }
},
"ambient": {
    "keyframes": [
        // 将正午的环境光提高
        {"hour": 14.0, "color": {"r": 0.45, "g": 0.45, "b": 0.45}}
    ]
}
```

### 示例 3: 恐怖氛围（昏暗）

降低整体亮度，增强对比。

```json
"sun": {
    "intensity": {
        "base_min": 0.2,
        "base_max": 0.5
    }
},
"moon": {
    "intensity": 0.1
},
"ambient": {
    "keyframes": [
        {"hour": 4.0, "color": {"r": 0.05, "g": 0.05, "b": 0.08}},
        {"hour": 14.0, "color": {"r": 0.25, "g": 0.25, "b": 0.25}}
    ]
}
```

## 实时调整

修改 `light_config.json` 后：
1. 保存文件
2. 重启游戏
3. 配置会在场景初始化时自动加载

如果配置文件加载失败，游戏会使用硬编码的默认值继续运行。

## 故障排除

**问题：配置不生效**
- 检查 JSON 格式是否正确（使用 JSON 验证工具）
- 确保文件位于 `assets/data/light_config.json`
- 查看游戏日志中的加载消息

**问题：颜色看起来不对**
- 颜色值范围是 0.0-1.0，不是 0-255
- RGB(128, 128, 128) 应写成 `{r:0.5, g:0.5, b:0.5}`

**问题：过渡不平滑**
- 增大 transition_periods 的时间跨度
- 确保环境光关键帧分布合理
