#ifndef DASMIG_EXT_ENTT_ADAPTER_HPP
#define DASMIG_EXT_ENTT_ADAPTER_HPP

#include "../entitygen.hpp"
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>

namespace dasmig::ext
{

/// @file entt_adapter.hpp
/// @brief Adapter bridging entity-generator with EnTT.

/// @brief Adapter that bridges entity-generator with EnTT.
///
/// Users register mappings from generator component keys to EnTT component
/// types, then spawn() converts generated entities into EnTT entities with
/// typed components.
///
/// @par Example
/// @code
///   entt::registry registry;
///   dasmig::ext::entt_adapter adapter(registry);
///   adapter.map<Position>(L"pos", [](const dasmig::entity& e) {
///       return Position{ e.get<float>(L"x"), e.get<float>(L"y") };
///   });
///   auto entt_entity = adapter.spawn(gen.generate());
/// @endcode
/// @see flecs_adapter
class entt_adapter
{
  public:
    /// @brief Construct an adapter bound to an EnTT registry.
    /// @param registry The EnTT registry to create entities in.
    explicit entt_adapter(entt::registry& registry)
        : _registry{registry} {}

    /// @brief Register a transform mapping from a generator key to an EnTT component.
    ///
    /// The mapper receives the full generated entity so it can read any
    /// combination of keys. The component is only emplaced when the
    /// specified key is present in the generated entity.
    /// @tparam Component The EnTT component type to emplace.
    /// @param key Generator component key to watch for.
    /// @param mapper Transform function producing the EnTT component.
    /// @return `*this` for chaining.
    template <typename Component>
    entt_adapter& map(const std::wstring& key,
                      std::function<Component(const entity&)> mapper)
    {
        _mappings.push_back({key,
            [mapper = std::move(mapper)](entt::registry& reg,
                                        entt::entity target,
                                        const entity& src) {
                reg.emplace<Component>(target, mapper(src));
            }});
        return *this;
    }

    /// @brief Register a direct mapping (value type matches EnTT component type).
    /// @tparam Component The EnTT component type.
    /// @param key Generator component key whose value type is @p Component.
    /// @return `*this` for chaining.
    template <typename Component>
    entt_adapter& map(const std::wstring& key)
    {
        _mappings.push_back({key,
            [key](entt::registry& reg, entt::entity target,
                  const entity& src) {
                reg.emplace<Component>(target,
                    src.get<Component>(key));
            }});
        return *this;
    }

    /// @brief Create an EnTT entity from a generated entity.
    ///
    /// Emplaces all mapped components whose keys are present.
    /// @param src The generated entity.
    /// @return The new EnTT entity.
    [[nodiscard]] entt::entity spawn(const entity& src)
    {
        auto target = _registry.create();
        apply(target, src);
        return target;
    }

    /// @brief Emplace mapped components onto an existing EnTT entity.
    /// @param target The pre-existing EnTT entity.
    /// @param src The generated entity.
    void spawn_into(entt::entity target, const entity& src)
    {
        apply(target, src);
    }

    /// @brief Create EnTT entities for a batch of generated entities.
    /// @param sources A vector of generated entities.
    /// @return A vector of new EnTT entities.
    [[nodiscard]] std::vector<entt::entity> spawn_batch(
        const std::vector<entity>& sources)
    {
        std::vector<entt::entity> result;
        result.reserve(sources.size());
        for (const auto& src : sources)
        {
            result.push_back(spawn(src));
        }
        return result;
    }

    /// @brief Remove all registered mappings.
    /// @return `*this` for chaining.
    entt_adapter& clear_mappings()
    {
        _mappings.clear();
        return *this;
    }

  private:
    using emplace_fn = std::function<void(entt::registry&, entt::entity,
                                          const entity&)>;

    struct mapping
    {
        std::wstring key;
        emplace_fn emplace;
    };

    void apply(entt::entity target, const entity& src)
    {
        for (const auto& m : _mappings)
        {
            if (src.has(m.key))
            {
                m.emplace(_registry, target, src);
            }
        }
    }

    entt::registry& _registry;
    std::vector<mapping> _mappings;
};

} // namespace dasmig::ext

#endif // DASMIG_EXT_ENTT_ADAPTER_HPP
