#include "../dasmig/entitygen.hpp"
#include <iostream>

// Example component: generates a random character class.
class character_class : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"class";
    }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        static const std::vector<std::wstring> classes{
            L"Warrior", L"Mage", L"Rogue", L"Healer", L"Ranger", L"Paladin"};

        return ctx.random().get(classes);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: generates a random age.
class age : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"age";
    }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.random().get(18, 65);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: generates a random level.
class level : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"level";
    }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        return ctx.random().get(1, 100);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: generates a random character name.
class character_name : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"name";
    }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        static const std::vector<std::wstring> names{
            L"Aldric",  L"Branwen", L"Cedric",  L"Dahlia",  L"Eldrin",
            L"Freya",   L"Gareth",  L"Helena",  L"Isolde",  L"Jareth",
            L"Kaelen",  L"Lyria",   L"Magnus",  L"Nadia",   L"Orion",
            L"Petra",   L"Quinlan", L"Rosalind"};

        return ctx.random().get(names);
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: demonstrates safe access to a previously generated
// component using ctx.has() and ctx.get().
class greeting : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"greeting";
    }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        // Guard against selective generation where "name" is absent.
        if (ctx.has(L"name"))
        {
            return L"Hello, " + ctx.get<std::wstring>(L"name") + L"!";
        }

        return std::wstring{L"Hello, stranger!"};
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        return default_to_string(value);
    }
};

// Example component: demonstrates a custom type with to_string() override
// and dependency on a previously generated component (level).
struct stats
{
    int strength;
    int agility;
    int intellect;
};

class character_stats : public dasmig::component
{
  public:
    [[nodiscard]] std::wstring key() const override
    {
        return L"stats";
    }

    [[nodiscard]] std::any generate(
        const dasmig::generation_context& ctx) const override
    {
        // Scale stat range based on level: higher level = higher potential.
        const int max_stat = ctx.has(L"level") ? ctx.get<int>(L"level") / 5 + 3 : 20;

        return stats{
            .strength  = ctx.random().get(1, max_stat),
            .agility   = ctx.random().get(1, max_stat),
            .intellect = ctx.random().get(1, max_stat)};
    }

    [[nodiscard]] std::wstring to_string(const std::any& value) const override
    {
        auto s = std::any_cast<stats>(value);
        return L"STR " + std::to_wstring(s.strength)
             + L" AGI " + std::to_wstring(s.agility)
             + L" INT " + std::to_wstring(s.intellect);
    }
};

int main()
{
    // Register components. Order matters: greeting depends on name.
    dasmig::eg::instance()
        .add(std::make_unique<character_name>())
        .add(std::make_unique<character_class>())
        .add(std::make_unique<age>())
        .add(std::make_unique<level>())
        .add(std::make_unique<character_stats>())
        .add(std::make_unique<greeting>());

    // Generate entities with all components.
    std::wcout << L"--- Full entities ---\n";
    for (std::size_t i = 0; i < 5; i++)
    {
        std::wcout << dasmig::eg::instance().generate() << L'\n';
    }

    // Generate entities with only specific components.
    std::wcout << L"\n--- Name and class only ---\n";
    for (std::size_t i = 0; i < 5; i++)
    {
        std::wcout << dasmig::eg::instance().generate({L"name", L"class"})
                   << L'\n';
    }

    // Typed retrieval.
    std::wcout << L"\n--- Typed access ---\n";
    auto entity = dasmig::eg::instance().generate();
    std::wcout << L"Name:     " << entity.get<std::wstring>(L"name") << L'\n';
    std::wcout << L"Class:    " << entity.get<std::wstring>(L"class") << L'\n';
    std::wcout << L"Age:      " << entity.get<int>(L"age") << L'\n';
    std::wcout << L"Level:    " << entity.get<int>(L"level") << L'\n';
    std::wcout << L"Greeting: " << entity.get<std::wstring>(L"greeting") << L'\n';

    // Custom type retrieval.
    auto s = entity.get<stats>(L"stats");
    std::wcout << L"Stats:    STR " << s.strength
               << L" AGI " << s.agility
               << L" INT " << s.intellect << L'\n';

    // Check if a component exists.
    std::wcout << L"Has stats? " << (entity.has(L"stats") ? L"yes" : L"no") << L'\n';

    // Reproducible generation with per-call seed.
    std::wcout << L"\n--- Seeded (same seed = same result) ---\n";
    std::wcout << dasmig::eg::instance().generate(42) << L'\n';
    std::wcout << dasmig::eg::instance().generate(42) << L'\n';

    // Generator-level seed for deterministic sequences.
    std::wcout << L"\n--- Generator seeded sequence ---\n";
    dasmig::eg::instance().seed(123);
    std::wcout << dasmig::eg::instance().generate() << L'\n';
    std::wcout << dasmig::eg::instance().generate() << L'\n';
    dasmig::eg::instance().unseed();

    // Selective generation with safe dependency (greeting without name).
    std::wcout << L"\n--- Greeting without name (safe fallback) ---\n";
    std::wcout << dasmig::eg::instance().generate({L"greeting"}) << L'\n';

    // Entity to_string().
    std::wcout << L"\n--- Entity to_string ---\n";
    std::wcout << dasmig::eg::instance().generate().to_string() << L'\n';

    // Batch generation.
    std::wcout << L"\n--- Batch generation (3 entities) ---\n";
    auto batch = dasmig::eg::instance().generate_batch(3);
    for (const auto& e : batch)
    {
        std::wcout << e << L'\n';
    }

    // Component groups.
    std::wcout << L"\n--- Component groups ---\n";
    dasmig::eg::instance().add_group(L"identity", {L"name", L"class"});
    std::wcout << dasmig::eg::instance().generate_group(L"identity") << L'\n';
    dasmig::eg::instance().remove_group(L"identity");

    // Entity validation: only accept entities where age > 30.
    std::wcout << L"\n--- Entity validation (age > 30) ---\n";
    dasmig::eg::instance().set_validator([](const dasmig::entity& e) {
        return e.get<int>(L"age") > 30;
    });
    auto validated = dasmig::eg::instance().generate();
    std::wcout << L"Age: " << validated.get<int>(L"age") << L'\n';
    dasmig::eg::instance().clear_validator();

    // Generation observer: hook into lifecycle events.
    class log_observer : public dasmig::generation_observer
    {
      public:
        void on_before_generate() override
        {
            std::wcout << L"  [observer] generating entity...\n";
        }
        void on_after_component(const std::wstring& key,
                                const std::any& /*value*/) override
        {
            std::wcout << L"  [observer] produced " << key << L'\n';
        }
    };

    std::wcout << L"\n--- Observer hooks ---\n";
    dasmig::eg::instance().set_observer(
        std::make_shared<log_observer>());
    dasmig::eg::instance().generate({L"name", L"class"});
    dasmig::eg::instance().clear_observer();

    // Conditional component: only generates when "class" is "Warrior".
    class warrior_title : public dasmig::component
    {
      public:
        [[nodiscard]] std::wstring key() const override { return L"title"; }

        [[nodiscard]] std::any generate(
            const dasmig::generation_context& ctx) const override
        {
            static const std::vector<std::wstring> titles{
                L"Warlord", L"Champion", L"Berserker"};
            return ctx.random().get(titles);
        }

        [[nodiscard]] bool should_generate(
            const dasmig::generation_context& ctx) const override
        {
            return ctx.has(L"class")
                && ctx.get<std::wstring>(L"class") == L"Warrior";
        }

        [[nodiscard]] std::wstring to_string(const std::any& value) const override
        {
            return default_to_string(value);
        }
    };

    std::wcout << L"\n--- Conditional component (title only for Warriors) ---\n";
    dasmig::eg::instance().add(std::make_unique<warrior_title>());
    for (std::size_t i = 0; i < 6; i++)
    {
        auto e = dasmig::eg::instance().generate({L"class", L"title"});
        std::wcout << e.get<std::wstring>(L"class");
        if (e.has(L"title"))
        {
            std::wcout << L" - " << e.get<std::wstring>(L"title");
        }
        std::wcout << L'\n';
    }
    dasmig::eg::instance().remove(L"title");

    return 0;
}
