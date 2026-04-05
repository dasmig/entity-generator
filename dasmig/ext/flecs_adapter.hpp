#ifndef DASMIG_EXT_FLECS_ADAPTER_HPP
#define DASMIG_EXT_FLECS_ADAPTER_HPP

#include "../entitygen.hpp"
#include <flecs.h>
#include <functional>
#include <string>
#include <vector>

namespace dasmig::ext
{

/// @file flecs_adapter.hpp
/// @brief Adapter bridging entity-generator with Flecs.

/// @brief Adapter that bridges entity-generator with Flecs.
///
/// Users register mappings from generator component keys to Flecs component
/// types, then spawn() converts generated entities into Flecs entities with
/// typed components.
///
/// @par Example
/// @code
///   flecs::world world;
///   dasmig::ext::flecs_adapter adapter(world);
///   adapter.map<Position>(L"pos", [](const dasmig::entity& e) {
///       return Position{ e.get<float>(L"x"), e.get<float>(L"y") };
///   });
///   auto flecs_entity = adapter.spawn(gen.generate());
/// @endcode
/// @see entt_adapter
class flecs_adapter
{
  public:
    /// @brief Construct an adapter bound to a Flecs world.
    /// @param world The Flecs world to create entities in.
    explicit flecs_adapter(flecs::world& world)
        : _world{world} {}

    /// @brief Register a transform mapping from a generator key to a Flecs component.
    ///
    /// The mapper receives the full generated entity so it can read any
    /// combination of keys. The component is only set when the specified
    /// key is present in the generated entity.
    /// @tparam Component The Flecs component type.
    /// @param key Generator component key to watch for.
    /// @param mapper Transform function producing the Flecs component.
    /// @return `*this` for chaining.
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

    /// @brief Register a direct mapping (value type matches Flecs component type).
    /// @tparam Component The Flecs component type.
    /// @param key Generator component key whose value type is @p Component.
    /// @return `*this` for chaining.
    template <typename Component>
    flecs_adapter& map(const std::wstring& key)
    {
        _mappings.push_back({key,
            [key](flecs::entity target, const entity& src) {
                target.set<Component>(src.get<Component>(key));
            }});
        return *this;
    }

    /// @brief Create a Flecs entity from a generated entity.
    ///
    /// Sets all mapped components whose keys are present.
    /// @param src The generated entity.
    /// @return The new Flecs entity.
    [[nodiscard]] flecs::entity spawn(const entity& src)
    {
        auto target = _world.entity();
        apply(target, src);
        return target;
    }

    /// @brief Set mapped components onto an existing Flecs entity.
    /// @param target The pre-existing Flecs entity.
    /// @param src The generated entity.
    void spawn_into(flecs::entity target, const entity& src)
    {
        apply(target, src);
    }

    /// @brief Create Flecs entities for a batch of generated entities.
    /// @param sources A vector of generated entities.
    /// @return A vector of new Flecs entities.
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

    /// @brief Remove all registered mappings.
    /// @return `*this` for chaining.
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
