#ifndef DASMIG_EXT_ENTT_ADAPTER_HPP
#define DASMIG_EXT_ENTT_ADAPTER_HPP

#include "../entitygen.hpp"
#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <vector>

namespace dasmig::ext
{

// Adapter that bridges entity-generator with EnTT. Users register mappings
// from generator component keys to EnTT component types, then spawn()
// converts generated entities into EnTT entities with typed components.
//
// Usage:
//   entt::registry registry;
//   dasmig::ext::entt_adapter adapter(registry);
//   adapter.map<Position>(L"pos", [](const dasmig::entity& e) {
//       return Position{ e.get<float>(L"x"), e.get<float>(L"y") };
//   });
//   auto entt_entity = adapter.spawn(gen.generate());
class entt_adapter
{
  public:
    explicit entt_adapter(entt::registry& registry)
        : _registry{registry} {}

    // Register a mapping from a generator key to an EnTT component type.
    // The mapper receives the full generated entity so it can read any
    // combination of keys. The component is only emplaced when the
    // specified key is present in the generated entity.
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

    // Register a direct mapping for keys whose generated value type
    // matches the EnTT component type exactly. No transform needed.
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

    // Create an EnTT entity from a generated entity, emplacing all
    // mapped components whose keys are present. Returns the EnTT entity.
    [[nodiscard]] entt::entity spawn(const entity& src)
    {
        auto target = _registry.create();
        apply(target, src);
        return target;
    }

    // Create an EnTT entity from a generated entity onto an existing
    // EnTT entity (useful for pre-created entities or prefabs).
    void spawn_into(entt::entity target, const entity& src)
    {
        apply(target, src);
    }

    // Create EnTT entities for a batch of generated entities.
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

    // Remove all registered mappings.
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
