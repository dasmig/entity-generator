#include "catch_amalgamated.hpp"
#include "../dasmig/entitygen.hpp"
#include "../dasmig/ext/flecs_adapter.hpp"

// ---------------------------------------------------------------------------
// Minimal test components (self-contained for this test file)
// ---------------------------------------------------------------------------

class string_component : public dasmig::component
{
  public:
    explicit string_component(std::wstring key, std::wstring value)
        : _key{std::move(key)}, _value{std::move(value)} {}
    [[nodiscard]] std::wstring key() const override { return _key; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context&) const override { return _value; }
    [[nodiscard]] std::wstring to_string(const std::any& v) const override
    { return default_to_string(v); }
  private:
    std::wstring _key;
    std::wstring _value;
};

class int_component : public dasmig::component
{
  public:
    explicit int_component(std::wstring key, int value)
        : _key{std::move(key)}, _value{value} {}
    [[nodiscard]] std::wstring key() const override { return _key; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context&) const override { return _value; }
    [[nodiscard]] std::wstring to_string(const std::any& v) const override
    { return default_to_string(v); }
  private:
    std::wstring _key;
    int _value;
};

// ---------------------------------------------------------------------------
// Flecs ECS component types (POD structs)
// ---------------------------------------------------------------------------

struct Name { std::wstring value; };
struct Age { int value; };
struct Position { float x; float y; };

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("flecs_adapter spawns entity with mapped components",
          "[flecs_adapter][spawn]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"Alice"))
       .add(std::make_unique<int_component>(L"age", 30));

    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });
    adapter.map<Age>(L"age", [](const dasmig::entity& e) {
        return Age{e.get<int>(L"age")};
    });

    auto flecs_entity = adapter.spawn(gen.generate());

    REQUIRE(flecs_entity.is_valid());
    REQUIRE(flecs_entity.has<Name>());
    REQUIRE(flecs_entity.has<Age>());
    REQUIRE(flecs_entity.get<Name>().value == L"Alice");
    REQUIRE(flecs_entity.get<Age>().value == 30);
}

TEST_CASE("flecs_adapter direct map without transform",
          "[flecs_adapter][map]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"age", 25));
    adapter.map<int>(L"age");

    auto flecs_entity = adapter.spawn(gen.generate());

    REQUIRE(flecs_entity.get<int>() == 25);
}

TEST_CASE("flecs_adapter skips unmapped keys",
          "[flecs_adapter][spawn]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"Bob"))
       .add(std::make_unique<int_component>(L"age", 42));

    // Only map "name", not "age"
    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });

    auto flecs_entity = adapter.spawn(gen.generate());

    REQUIRE(flecs_entity.has<Name>());
    REQUIRE_FALSE(flecs_entity.has<Age>());
}

TEST_CASE("flecs_adapter spawn_into uses existing entity",
          "[flecs_adapter][spawn_into]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"hp", 100));
    adapter.map<int>(L"hp");

    auto existing = world.entity();
    adapter.spawn_into(existing, gen.generate());

    REQUIRE(existing.get<int>() == 100);
}

TEST_CASE("flecs_adapter spawn_batch creates multiple entities",
          "[flecs_adapter][batch]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"NPC"));
    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });

    auto batch = gen.generate_batch(5);
    auto entities = adapter.spawn_batch(batch);

    REQUIRE(entities.size() == 5);
    for (auto& e : entities)
    {
        REQUIRE(e.is_valid());
        REQUIRE(e.get<Name>().value == L"NPC");
    }
}

TEST_CASE("flecs_adapter multi-key mapper reads multiple keys",
          "[flecs_adapter][map]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"x", 10))
       .add(std::make_unique<int_component>(L"y", 20));

    adapter.map<Position>(L"x", [](const dasmig::entity& e) {
        return Position{
            static_cast<float>(e.get<int>(L"x")),
            static_cast<float>(e.get<int>(L"y"))};
    });

    auto flecs_entity = adapter.spawn(gen.generate());

    const auto& pos = flecs_entity.get<Position>();
    REQUIRE(pos.x == 10.0f);
    REQUIRE(pos.y == 20.0f);
}

TEST_CASE("flecs_adapter clear_mappings removes all mappings",
          "[flecs_adapter][clear]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"age", 5));
    adapter.map<int>(L"age");
    adapter.clear_mappings();

    auto flecs_entity = adapter.spawn(gen.generate());

    REQUIRE_FALSE(flecs_entity.has<int>());
}

TEST_CASE("flecs_adapter spawn_batch with empty vector",
          "[flecs_adapter][batch]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);

    auto entities = adapter.spawn_batch({});
    REQUIRE(entities.empty());
}

TEST_CASE("flecs_adapter fluent chaining", "[flecs_adapter][api]")
{
    flecs::world world;
    dasmig::ext::flecs_adapter adapter(world);

    auto& ref = adapter
        .map<Name>(L"name", [](const dasmig::entity& e) {
            return Name{e.get<std::wstring>(L"name")};
        })
        .map<Age>(L"age", [](const dasmig::entity& e) {
            return Age{e.get<int>(L"age")};
        });

    REQUIRE(&ref == &adapter);
}
