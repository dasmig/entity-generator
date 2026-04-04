#include "catch_amalgamated.hpp"
#include "../dasmig/entitygen.hpp"
#include "../dasmig/ext/entt_adapter.hpp"

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

class half_weight_component : public dasmig::component
{
  public:
    explicit half_weight_component(std::wstring key, std::wstring value)
        : _key{std::move(key)}, _value{std::move(value)} {}
    [[nodiscard]] std::wstring key() const override { return _key; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context&) const override { return _value; }
    [[nodiscard]] std::wstring to_string(const std::any& v) const override
    { return default_to_string(v); }
    [[nodiscard]] double weight() const override { return 0.5; }
  private:
    std::wstring _key;
    std::wstring _value;
};

// ---------------------------------------------------------------------------
// EnTT ECS component types (POD structs)
// ---------------------------------------------------------------------------

struct Name { std::wstring value; };
struct Age { int value; };
struct Position { float x; float y; };

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("entt_adapter spawns entity with mapped components",
          "[entt_adapter][spawn]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"Alice"))
       .add(std::make_unique<int_component>(L"age", 30));

    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });
    adapter.map<Age>(L"age", [](const dasmig::entity& e) {
        return Age{e.get<int>(L"age")};
    });

    auto entt_entity = adapter.spawn(gen.generate());

    REQUIRE(registry.valid(entt_entity));
    REQUIRE(registry.all_of<Name, Age>(entt_entity));
    REQUIRE(registry.get<Name>(entt_entity).value == L"Alice");
    REQUIRE(registry.get<Age>(entt_entity).value == 30);
}

TEST_CASE("entt_adapter direct map without transform",
          "[entt_adapter][map]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"age", 25));
    adapter.map<int>(L"age");

    auto entt_entity = adapter.spawn(gen.generate());

    REQUIRE(registry.get<int>(entt_entity) == 25);
}

TEST_CASE("entt_adapter skips unmapped keys",
          "[entt_adapter][spawn]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"Bob"))
       .add(std::make_unique<int_component>(L"age", 42));

    // Only map "name", not "age"
    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });

    auto entt_entity = adapter.spawn(gen.generate());

    REQUIRE(registry.all_of<Name>(entt_entity));
    REQUIRE_FALSE(registry.all_of<Age>(entt_entity));
}

TEST_CASE("entt_adapter skips missing keys from weighted components",
          "[entt_adapter][spawn]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"Eve"))
       .add(std::make_unique<half_weight_component>(L"title", L"Dr"), 0.0);

    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });

    struct Title { std::wstring value; };
    adapter.map<Title>(L"title", [](const dasmig::entity& e) {
        return Title{e.get<std::wstring>(L"title")};
    });

    auto entt_entity = adapter.spawn(gen.generate());

    REQUIRE(registry.all_of<Name>(entt_entity));
    // weight 0.0 means "title" is always skipped
    REQUIRE_FALSE(registry.all_of<Title>(entt_entity));
}

TEST_CASE("entt_adapter spawn_into uses existing entity",
          "[entt_adapter][spawn_into]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"hp", 100));
    adapter.map<int>(L"hp");

    auto existing = registry.create();
    adapter.spawn_into(existing, gen.generate());

    REQUIRE(registry.get<int>(existing) == 100);
}

TEST_CASE("entt_adapter spawn_batch creates multiple entities",
          "[entt_adapter][batch]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<string_component>(L"name", L"NPC"));
    adapter.map<Name>(L"name", [](const dasmig::entity& e) {
        return Name{e.get<std::wstring>(L"name")};
    });

    auto batch = gen.generate_batch(5);
    auto entities = adapter.spawn_batch(batch);

    REQUIRE(entities.size() == 5);
    for (auto e : entities)
    {
        REQUIRE(registry.valid(e));
        REQUIRE(registry.get<Name>(e).value == L"NPC");
    }
}

TEST_CASE("entt_adapter multi-key mapper reads multiple keys",
          "[entt_adapter][map]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"x", 10))
       .add(std::make_unique<int_component>(L"y", 20));

    // Map two generator keys into one ECS component, triggered by "x"
    adapter.map<Position>(L"x", [](const dasmig::entity& e) {
        return Position{
            static_cast<float>(e.get<int>(L"x")),
            static_cast<float>(e.get<int>(L"y"))};
    });

    auto entt_entity = adapter.spawn(gen.generate());

    auto& pos = registry.get<Position>(entt_entity);
    REQUIRE(pos.x == 10.0f);
    REQUIRE(pos.y == 20.0f);
}

TEST_CASE("entt_adapter clear_mappings removes all mappings",
          "[entt_adapter][clear]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);
    dasmig::eg gen;

    gen.add(std::make_unique<int_component>(L"age", 5));
    adapter.map<int>(L"age");
    adapter.clear_mappings();

    auto entt_entity = adapter.spawn(gen.generate());

    // No mappings remain, so no components are emplaced
    REQUIRE_FALSE(registry.all_of<int>(entt_entity));
}

TEST_CASE("entt_adapter spawn_batch with empty vector",
          "[entt_adapter][batch]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);

    auto entities = adapter.spawn_batch({});
    REQUIRE(entities.empty());
}

TEST_CASE("entt_adapter fluent chaining", "[entt_adapter][api]")
{
    entt::registry registry;
    dasmig::ext::entt_adapter adapter(registry);

    // Verify map() returns *this for chaining
    auto& ref = adapter
        .map<Name>(L"name", [](const dasmig::entity& e) {
            return Name{e.get<std::wstring>(L"name")};
        })
        .map<Age>(L"age", [](const dasmig::entity& e) {
            return Age{e.get<int>(L"age")};
        });

    REQUIRE(&ref == &adapter);
}
