#ifndef DASMIG_EXT_FLECS_ADAPTER_HPP
#define DASMIG_EXT_FLECS_ADAPTER_HPP

#include "../entitygen.hpp"
#include <flecs.h>
#include <functional>
#include <string>
#include <vector>

namespace dasmig::ext
{

// Adapter that bridges entity-generator with Flecs. Users register mappings
// from generator component keys to Flecs component types, then spawn()
// converts generated entities into Flecs entities with typed components.
//
// Usage:
//   flecs::world world;
//   dasmig::ext::flecs_adapter adapter(world);
//   adapter.map<Position>(L"pos", [](const dasmig::entity& e) {
//       return Position{ e.get<float>(L"x"), e.get<float>(L"y") };
//   });
//   auto flecs_entity = adapter.spawn(gen.generate());
class flecs_adapter
{
  public:
    explicit flecs_adapter(flecs::world& world)
        : _world{world} {}

    // Register a mapping from a generator key to a Flecs component type.
    // The mapper receives the full generated entity so it can read any
    // combination of keys. The component is only set when the specified
    // key is present in the generated entity.
    template <typename Component>
    flecs_adapter& map(const std::wstring& key,
                       std::function<Component(const entity&)> mapper)
    {
        _mappings.push_back({key,
            [mapper = std::move(mapper)](flecs::entity target,
                                        const entity& src) {
                target.set<Component>(mapper(src));
            }});
        return *this;
    }

    // Register a direct mapping for keys whose generated value type
    // matches the Flecs component type exactly. No transform needed.
    template <typename Component>
    flecs_adapter& map(const std::wstring& key)
    {
        _mappings.push_back({key,
            [key](flecs::entity target, const entity& src) {
                target.set<Component>(src.get<Component>(key));
            }});
        return *this;
    }

    // Create a Flecs entity from a generated entity, setting all
    // mapped components whose keys are present. Returns the Flecs entity.
    [[nodiscard]] flecs::entity spawn(const entity& src)
    {
        auto target = _world.entity();
        apply(target, src);
        return target;
    }

    // Create a Flecs entity from a generated entity onto an existing
    // Flecs entity (useful for prefab instances or pre-created entities).
    void spawn_into(flecs::entity target, const entity& src)
    {
        apply(target, src);
    }

    // Create Flecs entities for a batch of generated entities.
    [[nodiscard]] std::vector<flecs::entity> spawn_batch(
        const std::vector<entity>& sources)
    {
        std::vector<flecs::entity> result;
        result.reserve(sources.size());
        for (const auto& src : sources)
        {
            result.push_back(spawn(src));
        }
        return result;
    }

    // Remove all registered mappings.
    flecs_adapter& clear_mappings()
    {
        _mappings.clear();
        return *this;
    }

  private:
    using set_fn = std::function<void(flecs::entity, const entity&)>;

    struct mapping
    {
        std::wstring key;
        set_fn apply_component;
    };

    void apply(flecs::entity target, const entity& src)
    {
        for (const auto& m : _mappings)
        {
            if (src.has(m.key))
            {
                m.apply_component(target, src);
            }
        }
    }

    flecs::world& _world;
    std::vector<mapping> _mappings;
};

} // namespace dasmig::ext

#endif // DASMIG_EXT_FLECS_ADAPTER_HPP
