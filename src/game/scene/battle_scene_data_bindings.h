#pragma once

namespace Rml {
class DataModelConstructor;
}

namespace game::scene {

/// @brief 注册 BattleScene RmlUi 数据模型使用的结构体类型。
[[nodiscard]] bool registerBattleSceneViewModelStructs(Rml::DataModelConstructor& constructor);

} // namespace game::scene
