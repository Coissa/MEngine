#pragma once

#include "Source/Core/Reflection/ReflectionMacros.h"

namespace ReflectionRendererTests
{
    class MENGINE_REFLECT_TYPE RenderSample
    {
        MENGINE_REFLECT_BODY(RenderSample)

    public:
        [[nodiscard]] int GetValue() const noexcept { return m_value; }

        [[nodiscard]] float GetWeight() const noexcept { return m_weight; }

    private:
        MENGINE_REFLECT_FIELD int m_value{42};
        MENGINE_REFLECT_FIELD const float m_weight{1.5F};
    };
} // namespace ReflectionRendererTests

#include <Runtime/Component.h>
#include <memory>

namespace ReflectionRendererTests
{
    enum class MENGINE_REFLECT_ENUM MENGINE_REFLECT_STABLE_NAME("Tests.Mode") Mode : signed char{Disabled = -1,
                                                                                                 Enabled = 1};

    struct MENGINE_REFLECT_TYPE MENGINE_REFLECT_STABLE_NAME("Tests.Settings") MENGINE_REFLECT_SERIALIZABLE Settings
    {
        MENGINE_REFLECT_BODY(Settings)
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("count") int count = 42;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_TRANSIENT int cache = 7;
    };

    class MENGINE_REFLECT_TYPE MENGINE_REFLECT_COMPONENT MENGINE_REFLECT_SERIALIZABLE GeneratedComponent final
        : public GE::Runtime::Component
    {
        MENGINE_COMPONENT_BODY(GeneratedComponent, "Tests.GeneratedComponent", 3)
    public:
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("settings") Settings settings;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("mode") Mode mode = Mode::Enabled;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("gain")
            MENGINE_REFLECT_EDITOR("Gain", 0.01, 0, 10) double gain = 1.2345678901234567;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("title") std::string title = "default";
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_TRANSIENT MENGINE_REFLECT_COPY int editor_selection = 5;
        int runtime_ticks = 0;
        std::unique_ptr<int> runtime_resource;
    };
} // namespace ReflectionRendererTests
