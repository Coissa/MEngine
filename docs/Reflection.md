# 反射、组件与持久化

组件声明是类型名称、版本、字段和注册信息的唯一来源。不要编辑 `.generated.*` 文件。`Tools/ReflectionGenerator/Manifests/Runtime.Reflection.json` 列出运行时反射头文件；同模块生成头的依赖自动连接，额外类型 traits 通过 `type_dependencies` 引入。

```cpp
class MENGINE_REFLECT_TYPE MENGINE_REFLECT_COMPONENT MENGINE_REFLECT_SERIALIZABLE Spinner final
    : public GE::Runtime::Component
{
    MENGINE_COMPONENT_BODY(Spinner, "Game.Spinner", 1)
public:
    MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("speed")
        MENGINE_REFLECT_EDITOR("Speed", 0.1, 0, 100) float speed = 30;
    float elapsed = 0; // 未反射的运行状态，不保存、不复制。
};
```

将组件所在头文件加入 manifest 后重新生成，即可获得字段访问器、TypeTraits、类型 ID、工厂注册、Save/Load 和配置复制实现。`MENGINE_REFLECT_ESSENTIAL` 表示对象必须拥有的组件；目前仅 Transform 使用。具体组件采用直接、非虚的 Component 单继承；复杂继承、裸指针、引用和原生数组字段不在支持范围内。

## 字段规则

- `MENGINE_REFLECT_FIELD`：供反射和属性面板访问。
- `MENGINE_REFLECT_SERIALIZED_NAME`：稳定的存储名称；C++ 成员改名时保留此名称。
- `MENGINE_REFLECT_REQUIRED`：读取时必须存在。普通新增字段不标 Required，缺失时使用构造默认值。
- `MENGINE_REFLECT_TRANSIENT`：不保存，加载时保留目标字段；默认也不进行配置复制。属性面板只读显示。
- `MENGINE_REFLECT_COPY`：显式允许 Transient 字段参与配置复制；不会使其进入存储格式。
- `MENGINE_REFLECT_EDITOR`：属性面板标签、拖动速度和数值范围；最小最大相同表示不限制范围。

配置复制逐字段执行，并递归处理反射结构。PIE 的新对象保留运行状态默认值；加载配置不会覆盖现有对象的运行状态、所属 World 或生命周期标记。只读字段可以反射显示，持久化字段必须可写。

支持所有基础整数、float/double、bool、string、枚举、Vec2/Vec3/Vec4、嵌套反射结构以及 AssetId 和场景资源编号。double 使用独立的 Archive 接口保存精度。枚举按名称存储。容器、任意指针对象图和复杂继承仍未开放，不能仅凭 TypeKind 的枚举项认为已经支持。

## 版本与职责

Component 声明纯虚 Save/Load 和配置复制接口；生成实现调用通用反射 Serializer。旧组件版本通过手写 LoadLegacy 迁移；默认实现拒绝不支持的版本。内置组件继续读取 payload v1，保存 payload v2；场景外层仍为 v4。未知字段和重复字段保持严格拒绝。字段类型、含义或枚举名称发生变化时应升级组件版本并编写迁移。

SceneSerializer 管理对象 ID、层级、资源表和文件版本，显式接收 Serialization::Context。ComponentRegistry 只管理组件描述、工厂和配置复制，不持有序列化服务。通用属性面板递归使用反射元数据；Transform、MeshRenderer、Camera 的专用编辑继续保留。

构建和生成命令见 [BUILDING.md](../BUILDING.md)。生成器测试包含真实组件的生成、编译、保存、加载、默认值、运行状态保留、配置复制、枚举和嵌套结构测试。
