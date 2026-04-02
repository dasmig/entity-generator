#include "catch_amalgamated.hpp"
#include "../dasmig/entitygen.hpp"
#include <sstream>

// ---------------------------------------------------------------------------
// Test components
// ---------------------------------------------------------------------------

class string_component : public dasmig::component
{
  public:
    explicit string_component(std::wstring key, std::wstring value)
        : _key{std::move(key)}, _value{std::move(value)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override
    {
        return _value;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

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
        const dasmig::generation_context& /*ctx*/) const override
    {
        return _value;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    std::wstring _key;
    int _value;
};

class random_int_component : public dasmig::component
{
  public:
    explicit random_int_component(std::wstring key, int lo, int hi)
        : _key{std::move(key)}, _lo{lo}, _hi{hi} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.random().get(_lo, _hi);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    std::wstring _key;
    int _lo;
    int _hi;
};

class dependent_component : public dasmig::component
{
  public:
    explicit dependent_component(std::wstring key, std::wstring depends_on)
        : _key{std::move(key)}, _depends_on{std::move(depends_on)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        if (ctx.has(_depends_on))
        {
            return L"dep:" + ctx.get<std::wstring>(_depends_on);
        }
        return std::wstring{L"dep:missing"};
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    std::wstring _key;
    std::wstring _depends_on;
};

struct custom_type
{
    int x;
    int y;

    bool operator==(const custom_type&) const = default;
};

class custom_type_component : public dasmig::component
{
  public:
    explicit custom_type_component(custom_type val) : _val{val} {}

    [[nodiscard]] std::wstring key() const override { return L"pos"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override
    {
        return _val;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        auto v = std::any_cast<custom_type>(value);
        return L"(" + std::to_wstring(v.x) + L"," + std::to_wstring(v.y) + L")";
    }

  private:
    custom_type _val;
};

// Component that reads an int from context and doubles it.
class doubler_component : public dasmig::component
{
  public:
    explicit doubler_component(std::wstring source_key)
        : _source{std::move(source_key)} {}

    [[nodiscard]] std::wstring key() const override { return L"doubled"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.get<int>(_source) * 2;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    std::wstring _source;
};

// Component that scales a random value by a context-provided level.
class scaled_component : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"scaled"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        int base = ctx.random().get(1, 10);
        int lvl = ctx.has(L"level") ? ctx.get<int>(L"level") : 1;
        return base * lvl;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Chain component: reads "doubled" and adds 1.
class chain_component : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"chain"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.get<int>(L"doubled") + 1;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Component that reports whether it can see a "later" key in context.
class forward_probe_component : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"probe"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        // "later" is registered after this component.
        return ctx.has(L"later");
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Component with a custom weight via the interface.
class weighted_component : public dasmig::component
{
  public:
    explicit weighted_component(std::wstring key, double w)
        : _key{std::move(key)}, _weight{w} {}

    [[nodiscard]] std::wstring key() const override { return _key; }
    [[nodiscard]] double weight() const override { return _weight; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override
    {
        return std::wstring{L"weighted"};
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    std::wstring _key;
    double _weight;
};

// Component that always fails validation.
class always_invalid_component : public dasmig::component
{
  public:
    explicit always_invalid_component(std::wstring key)
        : _key{std::move(key)} {}

    [[nodiscard]] std::wstring key() const override { return _key; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.random().get(1, 100);
    }

    [[nodiscard]] bool validate(const std::any& /*value*/) const override
    {
        return false;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    std::wstring _key;
};

// Component that only accepts even values.
class even_only_component : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"even"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.random().get(1, 1000);
    }

    [[nodiscard]] bool validate(const std::any& value) const override
    {
        return std::any_cast<int>(value) % 2 == 0;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Component that fails validation a fixed number of times before accepting.
class retry_count_component : public dasmig::component
{
  public:
    explicit retry_count_component(int fail_times) : _fail_times{fail_times} {}

    [[nodiscard]] std::wstring key() const override { return L"retry_comp"; }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override
    {
        return 0;
    }

    [[nodiscard]] bool validate(const std::any& /*value*/) const override
    {
        return ++_validate_count > _fail_times;
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }

  private:
    int _fail_times;
    mutable int _validate_count{0};
};

// ---------------------------------------------------------------------------
// Test observer: records lifecycle events for verification.
// ---------------------------------------------------------------------------

class test_observer : public dasmig::generation_observer
{
  public:
    std::vector<std::wstring> events;

    void on_before_generate() override
    { events.push_back(L"before_generate"); }
    void on_after_generate(const dasmig::entity&) override
    { events.push_back(L"after_generate"); }
    void on_before_component(const std::wstring& key) override
    { events.push_back(L"before_component:" + key); }
    void on_after_component(const std::wstring& key, const std::any&) override
    { events.push_back(L"after_component:" + key); }
    void on_before_skip(const std::wstring& key) override
    { events.push_back(L"before_skip:" + key); }
    void on_after_skip(const std::wstring& key) override
    { events.push_back(L"after_skip:" + key); }
    void on_before_retry(const std::wstring& key, std::size_t attempt) override
    { events.push_back(L"before_retry:" + key + L":" + std::to_wstring(attempt)); }
    void on_after_retry(const std::wstring& key, std::size_t attempt,
                        const std::any&) override
    { events.push_back(L"after_retry:" + key + L":" + std::to_wstring(attempt)); }
    void on_before_fail(const std::wstring& key) override
    { events.push_back(L"before_fail:" + key); }
    void on_after_fail(const std::wstring& key) override
    { events.push_back(L"after_fail:" + key); }
    void on_before_entity_retry(std::size_t attempt) override
    { events.push_back(L"before_entity_retry:" + std::to_wstring(attempt)); }
    void on_after_entity_retry(std::size_t attempt) override
    { events.push_back(L"after_entity_retry:" + std::to_wstring(attempt)); }
    void on_before_entity_fail() override
    { events.push_back(L"before_entity_fail"); }
    void on_after_entity_fail() override
    { events.push_back(L"after_entity_fail"); }
    void on_before_add(const std::wstring& key) override
    { events.push_back(L"before_add:" + key); }
    void on_after_add(const std::wstring& key) override
    { events.push_back(L"after_add:" + key); }
    void on_before_remove(const std::wstring& key) override
    { events.push_back(L"before_remove:" + key); }
    void on_after_remove(const std::wstring& key) override
    { events.push_back(L"after_remove:" + key); }
};

// ---------------------------------------------------------------------------
// Helper: clear all components from the singleton between tests.
// ---------------------------------------------------------------------------

struct clear_generator
{
    clear_generator()
    {
        auto& gen = dasmig::eg::instance();
        // Remove all known keys. Since we control the test components, we
        // remove by key. This is a workaround for the lack of a clear() API.
        for (const auto& k : std::vector<std::wstring>{
                 L"a", L"b", L"c", L"name", L"age", L"greeting", L"pos",
                 L"rand", L"level", L"doubled", L"scaled", L"chain",
                 L"probe", L"later", L"even"})
        {
            gen.remove(k);
        }
        gen.unseed();
        gen.clear_validator();
        gen.clear_observer();
        gen.max_retries(10);

        // Remove known groups.
        for (const auto& g : std::vector<std::wstring>{
                 L"combat", L"identity", L"nonexistent"})
        {
            gen.remove_group(g);
        }
    }
    ~clear_generator() = default;
};

// ---------------------------------------------------------------------------
// eg singleton
// ---------------------------------------------------------------------------

TEST_CASE("eg::instance returns the same reference", "[eg]")
{
    clear_generator guard;

    auto& a = dasmig::eg::instance();
    auto& b = dasmig::eg::instance();
    REQUIRE(&a == &b);
}

// ---------------------------------------------------------------------------
// Registration: add / remove / has
// ---------------------------------------------------------------------------

TEST_CASE("add and has", "[eg][registration]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    REQUIRE_FALSE(gen.has(L"a"));
    gen.add(std::make_unique<string_component>(L"a", L"hello"));
    REQUIRE(gen.has(L"a"));
}

TEST_CASE("add returns self for fluent chaining", "[eg][registration]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    auto& ret = gen.add(std::make_unique<string_component>(L"a", L"1"))
                   .add(std::make_unique<string_component>(L"b", L"2"));
    REQUIRE(&ret == &gen);
}

TEST_CASE("add replaces component with same key", "[eg][registration]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"old"));
    gen.add(std::make_unique<string_component>(L"a", L"new"));

    auto e = gen.generate();
    REQUIRE(e.get<std::wstring>(L"a") == L"new");
}

TEST_CASE("remove", "[eg][registration]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"hello"));
    REQUIRE(gen.has(L"a"));
    gen.remove(L"a");
    REQUIRE_FALSE(gen.has(L"a"));
}

TEST_CASE("remove returns self for fluent chaining", "[eg][registration]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"1"))
       .add(std::make_unique<string_component>(L"b", L"2"));

    auto& ret = gen.remove(L"a").remove(L"b");
    REQUIRE(&ret == &gen);
}

TEST_CASE("remove nonexistent key is a no-op", "[eg][registration]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.remove(L"nonexistent"); // should not throw
    REQUIRE_FALSE(gen.has(L"nonexistent"));
}

// ---------------------------------------------------------------------------
// Generate: all components
// ---------------------------------------------------------------------------

TEST_CASE("generate produces entity with all registered keys", "[eg][generate]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"alpha"))
       .add(std::make_unique<int_component>(L"b", 42));

    auto e = gen.generate();
    REQUIRE(e.has(L"a"));
    REQUIRE(e.has(L"b"));
    REQUIRE(e.get<std::wstring>(L"a") == L"alpha");
    REQUIRE(e.get<int>(L"b") == 42);
}

TEST_CASE("generate with no components produces empty entity", "[eg][generate]")
{
    clear_generator guard;

    auto e = dasmig::eg::instance().generate();
    REQUIRE(e.keys().empty());
}

// ---------------------------------------------------------------------------
// Generate: filtered
// ---------------------------------------------------------------------------

TEST_CASE("generate with specific keys", "[eg][generate][filtered]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"alpha"))
       .add(std::make_unique<string_component>(L"b", L"beta"))
       .add(std::make_unique<string_component>(L"c", L"gamma"));

    auto e = gen.generate({L"a", L"c"});
    REQUIRE(e.has(L"a"));
    REQUIRE_FALSE(e.has(L"b"));
    REQUIRE(e.has(L"c"));
}

TEST_CASE("generate with unknown keys skips silently", "[eg][generate][filtered]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"alpha"));

    auto e = gen.generate({L"a", L"nonexistent"});
    REQUIRE(e.has(L"a"));
    REQUIRE_FALSE(e.has(L"nonexistent"));
}

TEST_CASE("filtered generate preserves registration order", "[eg][generate][filtered]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"1"))
       .add(std::make_unique<string_component>(L"b", L"2"))
       .add(std::make_unique<string_component>(L"c", L"3"));

    // Request in reverse order.
    auto e = gen.generate({L"c", L"a"});
    auto keys = e.keys();
    REQUIRE(keys.size() == 2);
    REQUIRE(keys[0] == L"a");
    REQUIRE(keys[1] == L"c");
}

// ---------------------------------------------------------------------------
// Entity access
// ---------------------------------------------------------------------------

TEST_CASE("entity::get returns correct typed value", "[entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"name", L"Alice"))
       .add(std::make_unique<int_component>(L"age", 30));

    auto e = gen.generate();
    REQUIRE(e.get<std::wstring>(L"name") == L"Alice");
    REQUIRE(e.get<int>(L"age") == 30);
}

TEST_CASE("entity::get_any returns std::any reference", "[entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<int_component>(L"age", 25));
    auto e = gen.generate();

    const auto& val = e.get_any(L"age");
    REQUIRE(std::any_cast<int>(val) == 25);
}

TEST_CASE("entity::get throws on missing key", "[entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"hi"));
    auto e = gen.generate();

    REQUIRE_THROWS_AS(e.get<std::wstring>(L"missing"), std::out_of_range);
}

TEST_CASE("entity::get throws on wrong type", "[entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<int_component>(L"age", 10));
    auto e = gen.generate();

    REQUIRE_THROWS_AS(e.get<std::wstring>(L"age"), std::bad_any_cast);
}

TEST_CASE("entity::has", "[entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"val"));
    auto e = gen.generate();

    REQUIRE(e.has(L"a"));
    REQUIRE_FALSE(e.has(L"b"));
}

TEST_CASE("entity::keys preserves generation order", "[entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"b", L"2"))
       .add(std::make_unique<string_component>(L"a", L"1"))
       .add(std::make_unique<string_component>(L"c", L"3"));

    auto e = gen.generate();
    auto keys = e.keys();
    REQUIRE(keys.size() == 3);
    REQUIRE(keys[0] == L"b");
    REQUIRE(keys[1] == L"a");
    REQUIRE(keys[2] == L"c");
}

// ---------------------------------------------------------------------------
// generation_context: dependencies
// ---------------------------------------------------------------------------

TEST_CASE("generation_context: component can access earlier value", "[context]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"name", L"Alice"))
       .add(std::make_unique<dependent_component>(L"greeting", L"name"));

    auto e = gen.generate();
    REQUIRE(e.get<std::wstring>(L"greeting") == L"dep:Alice");
}

TEST_CASE("generation_context: ctx.has returns false for absent dep", "[context]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"name", L"Alice"))
       .add(std::make_unique<dependent_component>(L"greeting", L"name"));

    // Generate only greeting, without name.
    auto e = gen.generate({L"greeting"});
    REQUIRE(e.get<std::wstring>(L"greeting") == L"dep:missing");
}

TEST_CASE("generation_context: random engine is available", "[context]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 100));
    auto e = gen.generate();

    int val = e.get<int>(L"rand");
    REQUIRE(val >= 1);
    REQUIRE(val <= 100);
}

// ---------------------------------------------------------------------------
// Seeded generation
// ---------------------------------------------------------------------------

TEST_CASE("generate with same per-call seed produces identical entities", "[seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e1 = gen.generate(42);
    auto e2 = gen.generate(42);

    REQUIRE(e1.get<int>(L"rand") == e2.get<int>(L"rand"));
}

TEST_CASE("generate with different seeds produces different entities", "[seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e1 = gen.generate(1);
    auto e2 = gen.generate(2);

    // Probabilistically this should differ. With range 1-10000 and different
    // seeds, a collision is extremely unlikely.
    REQUIRE(e1.get<int>(L"rand") != e2.get<int>(L"rand"));
}

TEST_CASE("generator-level seed produces deterministic sequence", "[seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    gen.seed(99);
    auto a1 = gen.generate().get<int>(L"rand");
    auto a2 = gen.generate().get<int>(L"rand");

    gen.seed(99);
    auto b1 = gen.generate().get<int>(L"rand");
    auto b2 = gen.generate().get<int>(L"rand");

    REQUIRE(a1 == b1);
    REQUIRE(a2 == b2);
}

TEST_CASE("unseed makes generation non-deterministic again", "[seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 1000000));

    gen.seed(42);
    auto seeded = gen.generate().get<int>(L"rand");
    gen.unseed();

    // After unseed, re-seeding with same value should reproduce.
    gen.seed(42);
    auto reseeded = gen.generate().get<int>(L"rand");
    REQUIRE(seeded == reseeded);
}

TEST_CASE("seed returns self for fluent chaining", "[seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    auto& ret = gen.seed(1);
    REQUIRE(&ret == &gen);

    auto& ret2 = gen.unseed();
    REQUIRE(&ret2 == &gen);
}

TEST_CASE("filtered generate with seed", "[seed][filtered]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"a", 1, 10000))
       .add(std::make_unique<random_int_component>(L"b", 1, 10000));

    auto e1 = gen.generate({L"a"}, 77);
    auto e2 = gen.generate({L"a"}, 77);

    REQUIRE(e1.get<int>(L"a") == e2.get<int>(L"a"));
}

// ---------------------------------------------------------------------------
// Custom types
// ---------------------------------------------------------------------------

TEST_CASE("custom type round-trips through std::any", "[entity][custom]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<custom_type_component>(custom_type{10, 20}));
    auto e = gen.generate();

    auto val = e.get<custom_type>(L"pos");
    REQUIRE(val == custom_type{10, 20});
}

TEST_CASE("custom type to_string", "[entity][custom]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<custom_type_component>(custom_type{3, 7}));
    auto e = gen.generate();

    std::wostringstream wos;
    wos << e;
    auto output = wos.str();
    REQUIRE(output.find(L"(3,7)") != std::wstring::npos);
}

// ---------------------------------------------------------------------------
// Stream output
// ---------------------------------------------------------------------------

TEST_CASE("operator<< outputs all components", "[entity][stream]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"name", L"Bob"))
       .add(std::make_unique<int_component>(L"age", 25));

    auto e = gen.generate();
    std::wostringstream wos;
    wos << e;
    auto output = wos.str();

    REQUIRE(output.find(L"name: Bob") != std::wstring::npos);
    REQUIRE(output.find(L"age: 25") != std::wstring::npos);
}

TEST_CASE("operator<< respects generation order", "[entity][stream]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"b", L"second"))
       .add(std::make_unique<string_component>(L"a", L"first"));

    auto e = gen.generate();
    std::wostringstream wos;
    wos << e;
    auto output = wos.str();

    // "b" was registered first, so it should appear before "a".
    auto pos_b = output.find(L"b:");
    auto pos_a = output.find(L"a:");
    REQUIRE(pos_b < pos_a);
}

// ---------------------------------------------------------------------------
// default_to_string coverage
// ---------------------------------------------------------------------------

class bool_component : public dasmig::component
{
  public:
    explicit bool_component(bool val) : _val{val} {}
    [[nodiscard]] std::wstring key() const override { return L"b"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override { return _val; }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    { return default_to_string(value); }
  private:
    bool _val;
};

class double_component : public dasmig::component
{
  public:
    explicit double_component(double val) : _val{val} {}
    [[nodiscard]] std::wstring key() const override { return L"a"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override { return _val; }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    { return default_to_string(value); }
  private:
    double _val;
};

class float_component : public dasmig::component
{
  public:
    explicit float_component(float val) : _val{val} {}
    [[nodiscard]] std::wstring key() const override { return L"a"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override { return _val; }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    { return default_to_string(value); }
  private:
    float _val;
};

// Component producing a type not handled by default_to_string.
class unknown_type_component : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override { return L"a"; }
    [[nodiscard]] std::any generate(
        const dasmig::generation_context& /*ctx*/) const override
    { return std::vector<int>{1, 2, 3}; }
    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    { return default_to_string(value); }
};

TEST_CASE("default_to_string: bool true", "[to_string]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();
    gen.add(std::make_unique<bool_component>(true));

    std::wostringstream wos;
    wos << gen.generate();
    REQUIRE(wos.str().find(L"true") != std::wstring::npos);
}

TEST_CASE("default_to_string: bool false", "[to_string]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();
    gen.add(std::make_unique<bool_component>(false));

    std::wostringstream wos;
    wos << gen.generate();
    REQUIRE(wos.str().find(L"false") != std::wstring::npos);
}

TEST_CASE("default_to_string: double", "[to_string]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();
    gen.add(std::make_unique<double_component>(3.14));

    std::wostringstream wos;
    wos << gen.generate();
    REQUIRE(wos.str().find(L"3.14") != std::wstring::npos);
}

TEST_CASE("default_to_string: float", "[to_string]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();
    gen.add(std::make_unique<float_component>(2.5f));

    std::wostringstream wos;
    wos << gen.generate();
    REQUIRE(wos.str().find(L"2.5") != std::wstring::npos);
}

TEST_CASE("default_to_string: unknown type returns [?]", "[to_string]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();
    gen.add(std::make_unique<unknown_type_component>());

    std::wostringstream wos;
    wos << gen.generate();
    REQUIRE(wos.str().find(L"[?]") != std::wstring::npos);
}

// ---------------------------------------------------------------------------
// Context: typed dependency (int-based computation)
// ---------------------------------------------------------------------------

TEST_CASE("context: component doubles an int from a previous component", "[context][typed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<int_component>(L"level", 7))
       .add(std::make_unique<doubler_component>(L"level"));

    auto e = gen.generate();
    REQUIRE(e.get<int>(L"level") == 7);
    REQUIRE(e.get<int>(L"doubled") == 14);
}

TEST_CASE("context: component uses random scaled by previous int", "[context][typed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<int_component>(L"level", 5))
       .add(std::make_unique<scaled_component>());

    gen.seed(42);
    auto e = gen.generate();

    int val = e.get<int>(L"scaled");
    // base is 1..10, level is 5, so result is 5..50.
    REQUIRE(val >= 5);
    REQUIRE(val <= 50);
}

TEST_CASE("context: scaled component falls back when level absent", "[context][typed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<scaled_component>());

    gen.seed(42);
    auto e = gen.generate();

    int val = e.get<int>(L"scaled");
    // No level, so multiplier is 1. base is 1..10.
    REQUIRE(val >= 1);
    REQUIRE(val <= 10);
}

// ---------------------------------------------------------------------------
// Context: multi-hop dependency chain
// ---------------------------------------------------------------------------

TEST_CASE("context: three-component dependency chain", "[context][chain]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    // level(10) → doubled(20) → chain(21)
    gen.add(std::make_unique<int_component>(L"level", 10))
       .add(std::make_unique<doubler_component>(L"level"))
       .add(std::make_unique<chain_component>());

    auto e = gen.generate();
    REQUIRE(e.get<int>(L"level") == 10);
    REQUIRE(e.get<int>(L"doubled") == 20);
    REQUIRE(e.get<int>(L"chain") == 21);
}

// ---------------------------------------------------------------------------
// Context: does not leak forward
// ---------------------------------------------------------------------------

TEST_CASE("context: earlier component cannot see later component", "[context][ordering]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    // probe is registered before "later", so ctx.has("later") should be false.
    gen.add(std::make_unique<forward_probe_component>())
       .add(std::make_unique<string_component>(L"later", L"I exist"));

    auto e = gen.generate();
    REQUIRE(e.get<bool>(L"probe") == false);
}

// ---------------------------------------------------------------------------
// Context: ctx.get throws on missing key
// ---------------------------------------------------------------------------

TEST_CASE("context: ctx.get throws when dependency is absent", "[context][throws]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    // doubler reads "level", but level is not registered.
    gen.add(std::make_unique<doubler_component>(L"level"));

    REQUIRE_THROWS(gen.generate());
}

// ---------------------------------------------------------------------------
// Seeded context: same seed reproduces dependent values
// ---------------------------------------------------------------------------

TEST_CASE("seeded generation reproduces dependent computed values", "[context][seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<int_component>(L"level", 5))
       .add(std::make_unique<scaled_component>());

    auto e1 = gen.generate(99);
    auto e2 = gen.generate(99);

    REQUIRE(e1.get<int>(L"scaled") == e2.get<int>(L"scaled"));
}

// ---------------------------------------------------------------------------
// Component seed signature
// ---------------------------------------------------------------------------

TEST_CASE("entity::seed returns the seed used for a component", "[seed][signature]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e = gen.generate(42);
    // Should not throw — seed exists for every generated component.
    REQUIRE_NOTHROW(e.seed(L"rand"));
}

TEST_CASE("same generation seed produces same component seeds", "[seed][signature]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"a", 1, 10000))
       .add(std::make_unique<random_int_component>(L"b", 1, 10000));

    auto e1 = gen.generate(42);
    auto e2 = gen.generate(42);

    REQUIRE(e1.seed(L"a") == e2.seed(L"a"));
    REQUIRE(e1.seed(L"b") == e2.seed(L"b"));
}

TEST_CASE("different components get different seeds", "[seed][signature]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"a", 1, 10000))
       .add(std::make_unique<random_int_component>(L"b", 1, 10000));

    auto e = gen.generate(42);

    // Each component should be seeded independently.
    REQUIRE(e.seed(L"a") != e.seed(L"b"));
}

TEST_CASE("component seed reproduces the same value when used directly", "[seed][signature]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e = gen.generate(42);
    auto captured_seed = e.seed(L"rand");
    int original_value = e.get<int>(L"rand");

    // Manually seed a random engine with the captured seed and generate.
    effolkronium::random_local rng;
    rng.seed(static_cast<std::mt19937::result_type>(captured_seed));
    int replayed_value = rng.get(1, 10000);

    REQUIRE(original_value == replayed_value);
}

TEST_CASE("entity::seed throws on missing key", "[seed][signature]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"hi"));
    auto e = gen.generate();

    REQUIRE_THROWS_AS(e.seed(L"missing"), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Entity-level seed
// ---------------------------------------------------------------------------

TEST_CASE("entity::seed() returns the entity-level seed", "[seed][entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e = gen.generate(42);
    // Should not throw — every entity has a seed.
    REQUIRE_NOTHROW(e.seed());
    REQUIRE(e.seed() != 0);
}

TEST_CASE("same generation seed produces same entity seed", "[seed][entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e1 = gen.generate(42);
    auto e2 = gen.generate(42);

    REQUIRE(e1.seed() == e2.seed());
}

TEST_CASE("different generation seeds produce different entity seeds", "[seed][entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto e1 = gen.generate(1);
    auto e2 = gen.generate(2);

    REQUIRE(e1.seed() != e2.seed());
}

TEST_CASE("entity seed reproduces all component seeds", "[seed][entity]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"a", 1, 10000))
       .add(std::make_unique<random_int_component>(L"b", 1, 10000));

    auto e = gen.generate(42);

    // Derive component seeds from the entity seed the same way generate_impl does.
    std::mt19937 local_engine{static_cast<std::mt19937::result_type>(e.seed())};
    auto expected_seed_a = static_cast<std::uint64_t>(local_engine());
    auto expected_seed_b = static_cast<std::uint64_t>(local_engine());

    REQUIRE(e.seed(L"a") == expected_seed_a);
    REQUIRE(e.seed(L"b") == expected_seed_b);
}

// ---------------------------------------------------------------------------
// Batch generation
// ---------------------------------------------------------------------------

TEST_CASE("generate_batch produces correct number of entities", "[batch]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"val"));

    auto batch = gen.generate_batch(5);
    REQUIRE(batch.size() == 5);

    for (const auto& e : batch)
    {
        REQUIRE(e.has(L"a"));
    }
}

TEST_CASE("generate_batch with zero count returns empty vector", "[batch]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"val"));
    auto batch = gen.generate_batch(0);
    REQUIRE(batch.empty());
}

TEST_CASE("generate_batch with seed is deterministic", "[batch][seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto b1 = gen.generate_batch(3, 42);
    auto b2 = gen.generate_batch(3, 42);

    REQUIRE(b1.size() == 3);
    for (std::size_t i = 0; i < 3; ++i)
    {
        REQUIRE(b1[i].get<int>(L"rand") == b2[i].get<int>(L"rand"));
    }
}

TEST_CASE("generate_batch entities have distinct seeds", "[batch][seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000));

    auto batch = gen.generate_batch(3, 99);
    // Each entity in the batch should have a different entity seed.
    REQUIRE(batch[0].seed() != batch[1].seed());
    REQUIRE(batch[1].seed() != batch[2].seed());
}

// ---------------------------------------------------------------------------
// Component groups
// ---------------------------------------------------------------------------

TEST_CASE("add_group and has_group", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    REQUIRE_FALSE(gen.has_group(L"combat"));
    gen.add_group(L"combat", {L"class", L"level"});
    REQUIRE(gen.has_group(L"combat"));
}

TEST_CASE("add_group returns self for fluent chaining", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    auto& ret = gen.add_group(L"combat", {L"a"});
    REQUIRE(&ret == &gen);
}

TEST_CASE("remove_group", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add_group(L"combat", {L"a"});
    REQUIRE(gen.has_group(L"combat"));
    gen.remove_group(L"combat");
    REQUIRE_FALSE(gen.has_group(L"combat"));
}

TEST_CASE("remove_group returns self for fluent chaining", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add_group(L"combat", {L"a"});
    auto& ret = gen.remove_group(L"combat");
    REQUIRE(&ret == &gen);
}

TEST_CASE("generate_group produces entity with group keys only", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<string_component>(L"a", L"alpha"))
       .add(std::make_unique<string_component>(L"b", L"beta"))
       .add(std::make_unique<string_component>(L"c", L"gamma"));

    gen.add_group(L"identity", {L"a", L"c"});

    auto e = gen.generate_group(L"identity");
    REQUIRE(e.has(L"a"));
    REQUIRE_FALSE(e.has(L"b"));
    REQUIRE(e.has(L"c"));
}

TEST_CASE("generate_group with seed is deterministic", "[group][seed]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    gen.add(std::make_unique<random_int_component>(L"rand", 1, 10000))
       .add(std::make_unique<string_component>(L"name", L"X"));

    gen.add_group(L"combat", {L"rand"});

    auto e1 = gen.generate_group(L"combat", 42);
    auto e2 = gen.generate_group(L"combat", 42);

    REQUIRE(e1.get<int>(L"rand") == e2.get<int>(L"rand"));
}

TEST_CASE("generate_group throws on unknown group", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    REQUIRE_THROWS_AS(gen.generate_group(L"nonexistent"), std::out_of_range);
}

TEST_CASE("generate_group with seed throws on unknown group", "[group]")
{
    clear_generator guard;
    auto& gen = dasmig::eg::instance();

    REQUIRE_THROWS_AS(gen.generate_group(L"nonexistent", 42), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Independent instances
// ---------------------------------------------------------------------------

TEST_CASE("eg can be default constructed", "[instance]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"hello"));

    auto e = gen.generate();
    REQUIRE(e.has(L"a"));
    REQUIRE(e.get<std::wstring>(L"a") == L"hello");
}

TEST_CASE("independent instances do not share state", "[instance]")
{
    dasmig::eg gen1;
    dasmig::eg gen2;

    gen1.add(std::make_unique<string_component>(L"a", L"from_gen1"));
    gen2.add(std::make_unique<string_component>(L"b", L"from_gen2"));

    auto e1 = gen1.generate();
    auto e2 = gen2.generate();

    REQUIRE(e1.has(L"a"));
    REQUIRE_FALSE(e1.has(L"b"));
    REQUIRE(e2.has(L"b"));
    REQUIRE_FALSE(e2.has(L"a"));
}

TEST_CASE("independent instance does not affect singleton", "[instance]")
{
    clear_generator guard;

    dasmig::eg local;
    local.add(std::make_unique<string_component>(L"a", L"local"));

    REQUIRE_FALSE(dasmig::eg::instance().has(L"a"));
    REQUIRE(local.has(L"a"));
}

TEST_CASE("eg can be move constructed", "[instance]")
{
    dasmig::eg gen1;
    gen1.add(std::make_unique<string_component>(L"a", L"hello"));

    dasmig::eg gen2 = std::move(gen1);

    auto e = gen2.generate();
    REQUIRE(e.has(L"a"));
    REQUIRE(e.get<std::wstring>(L"a") == L"hello");
}

TEST_CASE("independent instance with seed is deterministic", "[instance][seed]")
{
    dasmig::eg gen1;
    gen1.add(std::make_unique<random_int_component>(L"rand", 1, 10000));
    gen1.seed(42);

    dasmig::eg gen2;
    gen2.add(std::make_unique<random_int_component>(L"rand", 1, 10000));
    gen2.seed(42);

    auto e1 = gen1.generate();
    auto e2 = gen2.generate();

    REQUIRE(e1.get<int>(L"rand") == e2.get<int>(L"rand"));
}

TEST_CASE("independent instance supports groups", "[instance][group]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"alpha"))
       .add(std::make_unique<string_component>(L"b", L"beta"));

    gen.add_group(L"identity", {L"a"});
    auto e = gen.generate_group(L"identity");

    REQUIRE(e.has(L"a"));
    REQUIRE_FALSE(e.has(L"b"));
}

TEST_CASE("independent instance supports batch", "[instance][batch]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"val"));

    auto batch = gen.generate_batch(3);
    REQUIRE(batch.size() == 3);
}

// ---------------------------------------------------------------------------
// Component weights
// ---------------------------------------------------------------------------

TEST_CASE("default component weight is 1.0", "[weight]")
{
    string_component comp(L"a", L"val");
    REQUIRE(comp.weight() == 1.0);
}

TEST_CASE("weighted_component returns custom weight", "[weight]")
{
    weighted_component comp(L"a", 0.5);
    REQUIRE(comp.weight() == 0.5);
}

TEST_CASE("weight 1.0 always includes component", "[weight]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"val"));

    // Weight defaults to 1.0, should always be present.
    for (int i = 0; i < 20; ++i)
    {
        auto e = gen.generate();
        REQUIRE(e.has(L"a"));
    }
}

TEST_CASE("weight 0.0 never includes component", "[weight]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<weighted_component>(L"a", 0.0));

    for (int i = 0; i < 20; ++i)
    {
        auto e = gen.generate();
        REQUIRE_FALSE(e.has(L"a"));
    }
}

TEST_CASE("weight override via add overload", "[weight]")
{
    dasmig::eg gen;
    // Component's own weight is 1.0, but we override to 0.0.
    gen.add(std::make_unique<string_component>(L"a", L"val"), 0.0);

    for (int i = 0; i < 20; ++i)
    {
        auto e = gen.generate();
        REQUIRE_FALSE(e.has(L"a"));
    }
}

TEST_CASE("weight override via weight method", "[weight]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    gen.weight(L"a", 0.0);

    for (int i = 0; i < 20; ++i)
    {
        auto e = gen.generate();
        REQUIRE_FALSE(e.has(L"a"));
    }
}

TEST_CASE("weight method throws for unregistered component", "[weight]")
{
    dasmig::eg gen;
    REQUIRE_THROWS_AS(gen.weight(L"nonexistent", 0.5), std::out_of_range);
}

TEST_CASE("weight override takes precedence over interface weight", "[weight]")
{
    dasmig::eg gen;
    // Component declares weight 0.0, but we override to 1.0.
    gen.add(std::make_unique<weighted_component>(L"a", 0.0), 1.0);

    for (int i = 0; i < 20; ++i)
    {
        auto e = gen.generate();
        REQUIRE(e.has(L"a"));
    }
}

TEST_CASE("remove clears weight override", "[weight]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"val"), 0.0);
    gen.remove(L"a");

    // Re-add without override; the component's own weight (1.0) applies.
    gen.add(std::make_unique<string_component>(L"a", L"val"));

    for (int i = 0; i < 20; ++i)
    {
        auto e = gen.generate();
        REQUIRE(e.has(L"a"));
    }
}

TEST_CASE("weighted component with seeded generation is deterministic", "[weight][seed]")
{
    dasmig::eg gen1;
    gen1.add(std::make_unique<weighted_component>(L"a", 0.5))
        .add(std::make_unique<string_component>(L"b", L"always"));

    dasmig::eg gen2;
    gen2.add(std::make_unique<weighted_component>(L"a", 0.5))
        .add(std::make_unique<string_component>(L"b", L"always"));

    auto e1 = gen1.generate(42);
    auto e2 = gen2.generate(42);

    REQUIRE(e1.has(L"a") == e2.has(L"a"));
    REQUIRE(e1.has(L"b") == e2.has(L"b"));
}

TEST_CASE("fractional weight includes component sometimes", "[weight]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<weighted_component>(L"a", 0.5));

    int included = 0;
    constexpr int trials = 200;
    for (int i = 0; i < trials; ++i)
    {
        auto e = gen.generate();
        if (e.has(L"a")) ++included;
    }

    // With weight 0.5 over 200 trials, expect roughly 100 inclusions.
    // Allow a generous range to avoid flaky tests.
    REQUIRE(included > 0);
    REQUIRE(included < trials);
}

TEST_CASE("weight applies in filtered generation", "[weight]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<weighted_component>(L"a", 0.0))
       .add(std::make_unique<string_component>(L"b", L"val"));

    auto e = gen.generate({L"a", L"b"});
    REQUIRE_FALSE(e.has(L"a"));
    REQUIRE(e.has(L"b"));
}

TEST_CASE("weight applies in group generation", "[weight][group]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<weighted_component>(L"a", 0.0))
       .add(std::make_unique<string_component>(L"b", L"val"));
    gen.add_group(L"grp", {L"a", L"b"});

    auto e = gen.generate_group(L"grp");
    REQUIRE_FALSE(e.has(L"a"));
    REQUIRE(e.has(L"b"));
}

TEST_CASE("weight applies in batch generation", "[weight][batch]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<weighted_component>(L"a", 0.0));

    auto batch = gen.generate_batch(5);
    for (const auto& e : batch)
    {
        REQUIRE_FALSE(e.has(L"a"));
    }
}

// ---------------------------------------------------------------------------
// entity::to_string
// ---------------------------------------------------------------------------

TEST_CASE("entity::to_string with single component", "[entity][to_string]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"name", L"Alice"));

    auto e = gen.generate();
    REQUIRE(e.to_string() == L"name: Alice");
}

TEST_CASE("entity::to_string with multiple components", "[entity][to_string]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"alpha"))
       .add(std::make_unique<int_component>(L"b", 42));

    auto e = gen.generate();
    REQUIRE(e.to_string() == L"a: alpha  b: 42");
}

TEST_CASE("entity::to_string on empty entity", "[entity][to_string]")
{
    dasmig::eg gen;
    auto e = gen.generate();
    REQUIRE(e.to_string().empty());
}

TEST_CASE("operator<< produces same output as to_string", "[entity][to_string]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"name", L"Bob"))
       .add(std::make_unique<int_component>(L"age", 25));

    auto e = gen.generate();
    std::wostringstream wos;
    wos << e;
    REQUIRE(wos.str() == e.to_string());
}

// ---------------------------------------------------------------------------
// Component-level validation
// ---------------------------------------------------------------------------

TEST_CASE("default validate always accepts", "[validation]")
{
    string_component comp(L"a", L"val");
    REQUIRE(comp.validate(std::any{std::wstring{L"val"}}));
}

TEST_CASE("component validate rejects and retries", "[validation]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<even_only_component>());

    // With retries, the generator should eventually produce an even value.
    auto e = gen.generate();
    REQUIRE(e.get<int>(L"even") % 2 == 0);
}

TEST_CASE("component validate throws after max retries", "[validation]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<always_invalid_component>(L"a"));
    gen.max_retries(3);

    REQUIRE_THROWS_AS(gen.generate(), std::runtime_error);
}

TEST_CASE("component validate with seeded generation", "[validation][seed]")
{
    dasmig::eg gen1;
    gen1.add(std::make_unique<even_only_component>());

    dasmig::eg gen2;
    gen2.add(std::make_unique<even_only_component>());

    auto e1 = gen1.generate(42);
    auto e2 = gen2.generate(42);

    REQUIRE(e1.get<int>(L"even") == e2.get<int>(L"even"));
    REQUIRE(e1.get<int>(L"even") % 2 == 0);
}

// ---------------------------------------------------------------------------
// Entity-level validation
// ---------------------------------------------------------------------------

TEST_CASE("set_validator returns self for fluent chaining", "[validation]")
{
    dasmig::eg gen;
    auto& ret = gen.set_validator([](const dasmig::entity&) { return true; });
    REQUIRE(&ret == &gen);
}

TEST_CASE("clear_validator returns self for fluent chaining", "[validation]")
{
    dasmig::eg gen;
    auto& ret = gen.clear_validator();
    REQUIRE(&ret == &gen);
}

TEST_CASE("max_retries returns self for fluent chaining", "[validation]")
{
    dasmig::eg gen;
    auto& ret = gen.max_retries(5);
    REQUIRE(&ret == &gen);
}

TEST_CASE("entity validator accepts valid entity", "[validation][entity]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<int_component>(L"age", 25));
    gen.set_validator([](const dasmig::entity& e) {
        return e.get<int>(L"age") > 0;
    });

    auto e = gen.generate();
    REQUIRE(e.get<int>(L"age") == 25);
}

TEST_CASE("entity validator retries until valid", "[validation][entity]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<random_int_component>(L"rand", 1, 100));
    gen.set_validator([](const dasmig::entity& e) {
        return e.get<int>(L"rand") > 50;
    });
    gen.max_retries(100);

    auto e = gen.generate();
    REQUIRE(e.get<int>(L"rand") > 50);
}

TEST_CASE("entity validator throws after max retries", "[validation][entity]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    gen.set_validator([](const dasmig::entity&) { return false; });
    gen.max_retries(3);

    REQUIRE_THROWS_AS(gen.generate(), std::runtime_error);
}

TEST_CASE("clear_validator removes validation", "[validation][entity]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    gen.set_validator([](const dasmig::entity&) { return false; });
    gen.clear_validator();

    // After clearing, generation should succeed.
    REQUIRE_NOTHROW(gen.generate());
}

TEST_CASE("entity validator applies to batch generation", "[validation][entity][batch]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<random_int_component>(L"rand", 1, 100));
    gen.set_validator([](const dasmig::entity& e) {
        return e.get<int>(L"rand") > 10;
    });
    gen.max_retries(100);

    auto batch = gen.generate_batch(5);
    for (const auto& e : batch)
    {
        REQUIRE(e.get<int>(L"rand") > 10);
    }
}

TEST_CASE("entity validator applies to group generation", "[validation][entity][group]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<random_int_component>(L"rand", 1, 100))
       .add(std::make_unique<string_component>(L"name", L"X"));
    gen.add_group(L"grp", {L"rand"});
    gen.set_validator([](const dasmig::entity& e) {
        return e.get<int>(L"rand") > 10;
    });
    gen.max_retries(100);

    auto e = gen.generate_group(L"grp");
    REQUIRE(e.get<int>(L"rand") > 10);
}

// ---------------------------------------------------------------------------
// Observer: API
// ---------------------------------------------------------------------------

TEST_CASE("set_observer returns self for fluent chaining", "[observer]")
{
    dasmig::eg gen;
    auto& ret = gen.set_observer(std::make_shared<test_observer>());
    REQUIRE(&ret == &gen);
}

TEST_CASE("clear_observer returns self for fluent chaining", "[observer]")
{
    dasmig::eg gen;
    auto& ret = gen.clear_observer();
    REQUIRE(&ret == &gen);
}

TEST_CASE("no observer does not crash", "[observer]")
{
    dasmig::eg gen;
    gen.add(std::make_unique<string_component>(L"a", L"hello"));
    REQUIRE_NOTHROW(gen.generate());
}

// ---------------------------------------------------------------------------
// Observer: entity lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after generate", "[observer]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    obs->events.clear();

    gen.generate();

    REQUIRE(obs->events.front() == L"before_generate");
    REQUIRE(obs->events.back() == L"after_generate");
}

// ---------------------------------------------------------------------------
// Observer: component lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after component in order", "[observer]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"1"));
    gen.add(std::make_unique<string_component>(L"b", L"2"));
    obs->events.clear();

    gen.generate();

    REQUIRE(obs->events.size() == 6);
    REQUIRE(obs->events[0] == L"before_generate");
    REQUIRE(obs->events[1] == L"before_component:a");
    REQUIRE(obs->events[2] == L"after_component:a");
    REQUIRE(obs->events[3] == L"before_component:b");
    REQUIRE(obs->events[4] == L"after_component:b");
    REQUIRE(obs->events[5] == L"after_generate");
}

// ---------------------------------------------------------------------------
// Observer: skip hooks
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after skip on weight exclusion", "[observer][weight]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"), 0.0);
    obs->events.clear();

    gen.generate();

    REQUIRE(obs->events[0] == L"before_generate");
    REQUIRE(obs->events[1] == L"before_skip:a");
    REQUIRE(obs->events[2] == L"after_skip:a");
    REQUIRE(obs->events[3] == L"after_generate");
}

// ---------------------------------------------------------------------------
// Observer: retry hooks
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after retry on validation retry", "[observer][validation]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<retry_count_component>(2));
    obs->events.clear();

    gen.generate();

    // retry_count_component fails validation 2 times, producing exactly 2 retries.
    REQUIRE(obs->events[0] == L"before_generate");
    REQUIRE(obs->events[1] == L"before_component:retry_comp");
    REQUIRE(obs->events[2] == L"before_retry:retry_comp:1");
    REQUIRE(obs->events[3] == L"after_retry:retry_comp:1");
    REQUIRE(obs->events[4] == L"before_retry:retry_comp:2");
    REQUIRE(obs->events[5] == L"after_retry:retry_comp:2");
    REQUIRE(obs->events[6] == L"after_component:retry_comp");
    REQUIRE(obs->events[7] == L"after_generate");
}

// ---------------------------------------------------------------------------
// Observer: component fail hooks
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after fail on validation exhaustion", "[observer][validation]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<always_invalid_component>(L"a"));
    gen.max_retries(2);
    obs->events.clear();

    REQUIRE_THROWS_AS(gen.generate(), std::runtime_error);

    // Last two events before the throw: before_fail, after_fail.
    auto it = std::ranges::find(obs->events, L"before_fail:a");
    REQUIRE(it != obs->events.end());
    ++it;
    REQUIRE(*it == L"after_fail:a");
}

// ---------------------------------------------------------------------------
// Observer: entity retry hooks
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after entity retry", "[observer][validation]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));

    int call_count = 0;
    gen.set_validator([&call_count](const dasmig::entity&) {
        return ++call_count > 1;
    });
    obs->events.clear();

    gen.generate();

    // Entity validator fails once, then succeeds. Expect 1 entity retry.
    REQUIRE(std::ranges::count(obs->events, L"before_entity_retry:1") == 1);
    REQUIRE(std::ranges::count(obs->events, L"after_entity_retry:1") == 1);
}

// ---------------------------------------------------------------------------
// Observer: entity fail hooks
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after entity fail", "[observer][validation]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    gen.set_validator([](const dasmig::entity&) { return false; });
    gen.max_retries(2);
    obs->events.clear();

    REQUIRE_THROWS_AS(gen.generate(), std::runtime_error);

    auto it = std::ranges::find(obs->events, L"before_entity_fail");
    REQUIRE(it != obs->events.end());
    ++it;
    REQUIRE(*it == L"after_entity_fail");
}

// ---------------------------------------------------------------------------
// Observer: registration hooks
// ---------------------------------------------------------------------------

TEST_CASE("observer receives before/after add on registration", "[observer][registration]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);

    gen.add(std::make_unique<string_component>(L"a", L"val"));

    REQUIRE(obs->events.size() == 2);
    REQUIRE(obs->events[0] == L"before_add:a");
    REQUIRE(obs->events[1] == L"after_add:a");
}

TEST_CASE("observer receives before/after remove on removal", "[observer][registration]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    obs->events.clear();

    gen.remove(L"a");

    REQUIRE(obs->events.size() == 2);
    REQUIRE(obs->events[0] == L"before_remove:a");
    REQUIRE(obs->events[1] == L"after_remove:a");
}

TEST_CASE("weight-override add fires hooks once", "[observer][registration]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);

    gen.add(std::make_unique<string_component>(L"a", L"val"), 0.5);

    REQUIRE(std::ranges::count(obs->events, L"before_add:a") == 1);
    REQUIRE(std::ranges::count(obs->events, L"after_add:a") == 1);
}

// ---------------------------------------------------------------------------
// Observer: clear_observer stops hooks
// ---------------------------------------------------------------------------

TEST_CASE("clear_observer stops hooks from firing", "[observer]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    obs->events.clear();

    gen.clear_observer();
    gen.generate();

    REQUIRE(obs->events.empty());
}

// ---------------------------------------------------------------------------
// Observer: seeded, batch, group generation
// ---------------------------------------------------------------------------

TEST_CASE("observer fires with seeded generation", "[observer][seed]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    obs->events.clear();

    gen.generate(42);

    REQUIRE(obs->events.front() == L"before_generate");
    REQUIRE(obs->events.back() == L"after_generate");
}

TEST_CASE("observer fires per entity in batch", "[observer][batch]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    obs->events.clear();

    gen.generate_batch(3);

    REQUIRE(std::ranges::count(obs->events, L"before_generate") == 3);
    REQUIRE(std::ranges::count(obs->events, L"after_generate") == 3);
}

TEST_CASE("observer fires with group generation", "[observer][group]")
{
    auto obs = std::make_shared<test_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);
    gen.add(std::make_unique<string_component>(L"a", L"1"))
       .add(std::make_unique<string_component>(L"b", L"2"));
    gen.add_group(L"grp", {L"a"});
    obs->events.clear();

    gen.generate_group(L"grp");

    REQUIRE(obs->events.front() == L"before_generate");
    REQUIRE(obs->events.back() == L"after_generate");
    // Only component "a" should appear, not "b".
    REQUIRE(std::ranges::count(obs->events, L"before_component:a") == 1);
    REQUIRE(std::ranges::count(obs->events, L"before_component:b") == 0);
}

TEST_CASE("default observer no-ops cover all hooks", "[observer]")
{
    auto obs = std::make_shared<dasmig::generation_observer>();
    dasmig::eg gen;
    gen.set_observer(obs);

    // Registration + removal hooks (default no-ops).
    gen.add(std::make_unique<string_component>(L"a", L"val"), 0.0);
    gen.remove(L"a");

    // Generate + skip + component hooks.
    gen.add(std::make_unique<string_component>(L"a", L"val"), 0.0);
    gen.add(std::make_unique<string_component>(L"b", L"val"));
    gen.generate();
    gen.remove(L"a");
    gen.remove(L"b");

    // Retry hooks.
    gen.add(std::make_unique<retry_count_component>(1));
    gen.generate();
    gen.remove(L"retry_comp");

    // Component fail hooks.
    gen.add(std::make_unique<always_invalid_component>(L"a"));
    gen.max_retries(1);
    REQUIRE_THROWS(gen.generate());
    gen.remove(L"a");

    // Entity retry + entity fail hooks.
    gen.add(std::make_unique<string_component>(L"a", L"val"));
    gen.set_validator([](const dasmig::entity&) { return false; });
    gen.max_retries(1);
    REQUIRE_THROWS(gen.generate());
}
