#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "engine/ui/ui_button.h"

namespace engine::ui {

TEST(UIButtonFactoryApiTest, CreateReturnsUniquePtr) {
    using ReturnType = decltype(UIButton::create(std::declval<engine::core::Context&>(), entt::id_type{}));
    static_assert(std::is_same_v<ReturnType, std::unique_ptr<UIButton>>);
    SUCCEED();
}

} // namespace engine::ui

