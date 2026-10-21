// A small village: entity-generator makes the villagers, the EnTT adapter turns
// them into registry entities, and plain EnTT views run over them.
//
// It also checks what the adapter has to keep for the generator's promises to
// hold in an ECS: the same seed gives the same registry (for any thread count),
// and a registry entity replays from the seed it carries.
//
//   clang++ -std=c++23 -I. -I<path to entt/src> examples/entt_demo.cpp -o entt_demo
//
// Exits with 0 when every check passes.

#include "../dasmig/entitygen.hpp"
#include "../dasmig/ext/entt_adapter.hpp"

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace
{

namespace core = dasmig::core;

// --- ECS components --------------------------------------------------------

struct Name { std::wstring value; };
struct Age { int years; };
struct Health { int hp; int max_hp; };
struct Occupation { std::wstring job; };
struct Faction { std::wstring name; };
struct Elder {}; // a tag: added by a system, not by the generator

// What a registry entity needs in order to be regenerated.
struct Origin { core::seed_signature signature; };

// --- Generator setup --------------------------------------------------------

// The village's factions depend on a request input, so a call can ask for a
// different region without rebuilding the generator.
void build_village(dasmig::eg& gen)
{
    gen.add(dasmig::field(L"name", [](const dasmig::generation_context& ctx)
                          {
                              static const std::vector<std::wstring> given{L"Ana", L"Bo", L"Cyra", L"Dov", L"Eli", L"Fen"};
                              static const std::vector<std::wstring> family{L"Marsh", L"Hale", L"Voss", L"Reed"};
                              return core::pick(ctx.random(), given) + L" " + core::pick(ctx.random(), family);
                          }));
    gen.add(dasmig::field(L"age", [](const dasmig::generation_context& ctx)
                          { return static_cast<int>(core::uniform_int(ctx.random(), 8, 85)); }));
    gen.add(dasmig::field(L"job", [](const dasmig::generation_context& ctx)
                          {
                              const int age = ctx.get<int>(L"age");
                              if (age < 14) return std::wstring{L"Child"};
                              static const std::vector<std::wstring> jobs{L"Farmer", L"Smith", L"Baker", L"Guard"};
                              return age > 65 ? std::wstring{L"Retired"} : core::pick(ctx.random(), jobs);
                          })
               .depends_on({L"age"}));
    // Health scales with the job and age; a child is frail, a guard is tough.
    gen.add(dasmig::field(L"hp", [](const dasmig::generation_context& ctx)
                          {
                              const int base = ctx.get<std::wstring>(L"job") == L"Guard" ? 120 : 80;
                              const int age = ctx.get<int>(L"age");
                              const int scaled = age < 14 ? base / 2 : (age > 65 ? base * 3 / 4 : base);
                              return static_cast<int>(scaled + core::uniform_int(ctx.random(), -10, 10));
                          })
               .depends_on({L"age", L"job"}));
    gen.add(dasmig::field(L"faction", [](const dasmig::generation_context& ctx)
                          {
                              const auto region = ctx.input_or<std::string>("region", "north");
                              static const std::vector<std::wstring> north{L"Pinewatch", L"Ironvale"};
                              static const std::vector<std::wstring> south{L"Sunreach", L"Saltmarsh"};
                              return core::pick(ctx.random(), region == "south" ? south : north);
                          }));
    // Only some villagers keep a pet; the component is skipped for the rest.
    gen.add(dasmig::field(L"pet", [](const dasmig::generation_context& ctx)
                          {
                              static const std::vector<std::wstring> pets{L"dog", L"cat", L"goat"};
                              return core::pick(ctx.random(), pets);
                          })
               .when([](const dasmig::generation_context& ctx) { return ctx.get<int>(L"age") >= 14; })
               .depends_on({L"age"}));
}

struct Pet { std::wstring kind; };

void map_village(dasmig::ext::entt_adapter& adapter)
{
    adapter.map<Name>(L"name", [](const dasmig::entity& e) { return Name{e.get<std::wstring>(L"name")}; })
        .map<Age>(L"age", [](const dasmig::entity& e) { return Age{e.get<int>(L"age")}; })
        .map<Occupation>(L"job", [](const dasmig::entity& e) { return Occupation{e.get<std::wstring>(L"job")}; })
        .map<Health>(L"hp", [](const dasmig::entity& e) { return Health{e.get<int>(L"hp"), e.get<int>(L"hp")}; })
        .map<Faction>(L"faction", [](const dasmig::entity& e) { return Faction{e.get<std::wstring>(L"faction")}; })
        .map<Pet>(L"pet", [](const dasmig::entity& e) { return Pet{e.get<std::wstring>(L"pet")}; });
}

// --- The checks --------------------------------------------------------------

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

// A fingerprint of everything in a registry, in entity order.
std::uint64_t fingerprint(entt::registry& registry)
{
    std::uint64_t h = 1469598103934665603ULL;
    const auto mix = [&h](std::uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
    registry.view<Name, Age, Health, Occupation, Faction>().each(
        [&](const Name& n, const Age& a, const Health& hp, const Occupation& o, const Faction& f)
        {
            for (const wchar_t c : n.value + o.job + f.name) mix(static_cast<std::uint64_t>(c));
            mix(static_cast<std::uint64_t>(a.years));
            mix(static_cast<std::uint64_t>(hp.hp));
        });
    return h;
}

// Spawns a batch of villagers: the generator makes them (on `threads` threads),
// the adapter fills the registry on this one, and each keeps its signature.
std::size_t populate(entt::registry& registry, const dasmig::eg& gen, const dasmig::entity_request& request,
                     std::uint64_t seed, std::size_t count, std::size_t threads)
{
    dasmig::ext::entt_adapter adapter{registry};
    map_village(adapter);
    std::size_t made = 0;
    gen.for_each_entity(request, {.count = count, .threads = threads}, seed,
                        [&](std::uint64_t, dasmig::entity& e)
                        {
                            const auto id = adapter.spawn(e);
                            registry.emplace<Origin>(id, Origin{e.signature()});
                            ++made;
                        });
    return made;
}

} // namespace

int main()
{
    dasmig::eg gen;
    build_village(gen);
    constexpr std::uint64_t seed = 0x5eed;
    constexpr std::size_t population = 2000;
    dasmig::entity_request north{.inputs = {{"region", std::string{"north"}}}};
    dasmig::entity_request south{.inputs = {{"region", std::string{"south"}}}};

    std::puts("Populate");
    entt::registry village;
    check(populate(village, gen, north, seed, population, 1) == population, "2000 villagers spawned");
    check(village.view<Name>().size() == population, "every entity has a Name");
    check(village.view<Pet>().size() > 0 && village.view<Pet>().size() < population,
          "Pet is on some entities only (a skipped component emplaces nothing)");

    std::puts("Views");
    std::size_t children_with_pets = 0, elders = 0, hp_sum = 0;
    village.view<Age, Occupation>().each(
        [&](entt::entity id, const Age& age, const Occupation& job)
        {
            if (age.years < 14 && village.all_of<Pet>(id)) ++children_with_pets;
            if (job.job == L"Retired") { village.emplace<Elder>(id); ++elders; }
        });
    village.view<Health>().each([&](const Health& h) { hp_sum += static_cast<std::size_t>(h.hp); });
    check(children_with_pets == 0, "no child has a pet (the .when() condition held)");
    check(village.view<Elder>().size() == elders && elders > 0, "a system tagged the retired as Elder");
    std::printf("      average hp %.1f over %zu villagers\n", static_cast<double>(hp_sum) / population, population);

    std::puts("Faction follows the request input");
    entt::registry southern;
    populate(southern, gen, south, seed, 200, 1);
    bool all_south = true;
    southern.view<Faction>().each([&](const Faction& f) { all_south = all_south && (f.name == L"Sunreach" || f.name == L"Saltmarsh"); });
    check(all_south, "region=south gives southern factions only");

    std::puts("Determinism");
    entt::registry again, threaded;
    populate(again, gen, north, seed, population, 1);
    populate(threaded, gen, north, seed, population, 4);
    check(fingerprint(again) == fingerprint(village), "same seed, same registry");
    check(fingerprint(threaded) == fingerprint(village), "4 generator threads, same registry");
    entt::registry other;
    populate(other, gen, north, seed + 1, population, 1);
    check(fingerprint(other) != fingerprint(village), "another seed, another registry");

    std::puts("Replay from the registry");
    bool replays = true;
    std::size_t checked = 0;
    village.view<Origin, Name, Age>().each(
        [&](const Origin& origin, const Name& name, const Age& age)
        {
            if (checked++ >= 100) return;
            const auto replay = gen.generate(north, origin.signature.seed);
            replays = replays && replay.get<std::wstring>(L"name") == name.value && replay.get<int>(L"age") == age.years;
        });
    check(replays, "100 entities replay from the seed in their Origin");

    std::puts("Adapter edge cases");
    {
        entt::registry registry;
        dasmig::ext::entt_adapter adapter{registry};
        // The key holds an int, the mapping asks for a string: a mistake in the mapping.
        adapter.map<Name>(L"name").map<Age>(L"age"); // direct maps: Name is not a std::wstring
        const auto before = registry.storage<entt::entity>().free_list();
        bool threw = false;
        try { static_cast<void>(adapter.spawn(gen.generate(north, 1))); }
        catch (const std::exception&) { threw = true; }
        check(threw, "a mapping of the wrong type throws at spawn");
        check(registry.storage<entt::entity>().free_list() == before,
              "a throwing spawn leaves no half-built entity behind");
    }
    {
        entt::registry registry;
        dasmig::ext::entt_adapter adapter{registry};
        adapter.map<Age>(L"age", [](const dasmig::entity& e) { return Age{e.get<int>(L"age")}; });
        const auto e = gen.generate({.component_keys = std::vector<std::wstring>{L"name"}}, 1);
        const auto id = adapter.spawn(e);
        check(registry.valid(id) && !registry.all_of<Age>(id), "a key the request left out emplaces nothing");
    }

    std::printf("\n%s\n", failures == 0 ? "All checks passed." : "Some checks failed.");
    return failures == 0 ? 0 : 1;
}
