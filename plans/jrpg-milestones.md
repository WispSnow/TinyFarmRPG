# 拓展JRPG开发顺序

当前状态补充

- Milestone C 已完成：最小任务闭环已支持 `接任务 / 杀怪计数 / 回 NPC 交付 / 存档恢复`。
- Milestone D 已完成：最小商店闭环已支持 `merchant 交互 / buy / sell / 热键栏同步 / 键盘手柄导航 / save roundtrip`，详见 `plans/jrpg-milestone-d-shop-index.md`。
- 当前推荐的下一阶段主线回到战斗可玩化与技能接入，也就是原先的 Milestone A / B 方向。

- 1. 战斗可玩化 + 技能接入，作为下一阶段主目标。
  现在这条线基础最好，已经有完整领域层、场景层、文档和测试骨架，见 turn-based-battle.md、
  battle_scene.cpp:28。真正缺的是把临时硬编码替换成玩家可选的 Skill / Item / Target UI，因为当前仍写死了
  默认技能和默认物品，见 battle_scene.cpp:28。
- 2. 任务线 MVP，放在战斗之后。
  任务存档位已经预留好了，但运行时系统基本还没落地，见 save_data.h:112。这说明它适合做成“第二步的骨架系
  统”，而不是现在直接冲全量内容。建议先做 quest catalog + active/completed/progress tracker + 事件驱动目
  标推进 + 对话触发/提交。
- 3. 商店，放在任务之后。
  现在金币、掉落、任务奖励与背包显示都已经接通，商店的前置条件基本具备。下一步的重点不再是“先补货币来源”，
  而是锁定 merchant 入口、shop 目录、交易原子性和 buy/sell UI，详见
  `plans/jrpg-milestone-d-shop-index.md`。
- 4. 技能系统不要单独立项，而应并入战斗阶段。
  技能数据加载、查表、战斗解析其实已经有不少基础，见 rpg_catalog.cpp:291。更合理的目标不是先做“完整技能
  树”，而是先完成“角色已学技能 -> 战斗菜单可选 -> 消耗 MP -> 结算效果 -> 存档恢复”这条闭环。

为什么这样排

- 战斗线已有最多现成资产，产出最快，最符合“尽快做出 JRPG Demo”的目标。
- 任务线能把探索、对话、战斗串成真正的主循环。
- 任务线先把探索、对话、战斗、回报主循环锁定下来，再补商店的金币消耗与买卖闭环更稳。
- 物品栏菜单已经完成，当前最合理的动作是让它服务于玩法闭环，而不是继续做菜单层扩展。

我会推荐的下一个具体里程碑

- Milestone A: 战斗菜单从硬编码按钮升级为真实 Skill / Item / Target 选择。
- Milestone B: 敌人 AI + 战斗奖励 + 金币/掉落入包。
- Milestone C: 最小任务系统，先支持“接任务 / 杀怪计数 / 交付完成”。
- Milestone D: 商店 UI 和买卖规则，已完成最小闭环，见 `plans/jrpg-milestone-d-shop-index.md`。

Milestone A 执行约束

- 战斗菜单 UI 实现应遵循当前 RmlUi 集成方式：生产场景使用 `RmlDocumentController` 管理文档、data model、事件绑定和脏标记，不回退到旧的直接 `RmlDataBridge` / `loadRmlDocument()` 路径。
- 键盘 / 手柄移动、确认与返回应按 `InputContext::Battle` 的 `menu_up/down/left/right/confirm/cancel` 场景输入处理；鼠标点击继续走 RML `data-event-click`。
- 战斗菜单 Stage 1 采用 `BattleScene` 自主管理光标并程序化同步 RmlUi 焦点的方案，不依赖 RmlUi 原生方向键导航。
- 物品菜单若进入“真实消耗”阶段，必须同时定义战斗运行时 `item_stocks` 与真实玩家背包之间的同步 / 写回路径。
