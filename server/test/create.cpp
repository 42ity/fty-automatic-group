#include <catch2/catch.hpp>
#include "lib/jobs/create.h"

#include "db.h"


TEST_CASE("Create")
{
    REQUIRE(fty::GroupsDB::init());

    SECTION("Normal")
    {
        // Create any group
        fty::Group group;
        group.name          = "My group";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        {
            auto& var  = group.rules.conditions.append();
            auto& cond = var.reset<fty::Group::Condition>();
            cond.value = "srv";
            cond.field = fty::Group::Fields::Name;
            cond.op    = fty::Group::ConditionOp::Contains;
        }

        fty::job::Create create;
        fty::Group created;
        REQUIRE_NOTHROW(create.run(group, created));
        CHECK(created.hasValue());
        CHECK(created.id > 0);
        CHECK(created.name == group.name);
        CHECK(created.rules == group.rules);
    }

    SECTION("From json")
    {
        std::string json = R"(
            {
                "name": "By types",
                "rules": {
                    "conditions": [
                        {
                            "field": "type",
                            "operator": "CONTAINS",
                            "value": "srv"
                        }
                    ],
                    "operator": "OR"
                }
            }
        )";
        fty::Group group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group created;
        REQUIRE_NOTHROW(create.run(group, created));

        CHECK(created.hasValue());
        CHECK(created.id > 0);
        CHECK(created.name == "By types");
        CHECK(created.rules.groupOp == fty::Group::LogicalOp::Or);
        REQUIRE(created.rules.conditions.size() == 1);
        REQUIRE_NOTHROW(created.rules.conditions[0].get<fty::Group::Condition>());
        auto& cond = created.rules.conditions[0].get<fty::Group::Condition>();
        CHECK(cond.field == fty::Group::Fields::Type);
        CHECK(cond.op == fty::Group::ConditionOp::Contains);
        CHECK(cond.value == "srv");
    }

    SECTION("From json, wrong condition op")
    {
        std::string json = R"(
            {
                "name": "By types",
                "rules": {
                    "conditions": [
                        {
                            "field": "type",
                            "operator": "AND",
                            "value": "srv"
                        }
                    ],
                    "operator": "OR"
                }
            }
        )";
        fty::Group group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group created;
        REQUIRE_THROWS_WITH(create.run(group, created), "Valid condition operator is expected");
    }

    SECTION("From json, wrong rules op")
    {
        std::string json = R"(
            {
                "name": "By types",
                "rules": {
                    "conditions": [
                        {
                            "field": "type",
                            "operator": "IS",
                            "value": "srv"
                        }
                    ],
                    "operator": "WTF"
                }
            }
        )";
        fty::Group group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group created;
        REQUIRE_THROWS_WITH(create.run(group, created), "Valid logical operator is expected");
    }

    SECTION("From json, empty name")
    {
        std::string json = R"(
            {
                "rules": {
                    "conditions": [
                        {
                            "field": "type",
                            "operator": "IS",
                            "value": "srv"
                        }
                    ],
                    "operator": "WTF"
                }
            }
        )";
        fty::Group group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group created;
        REQUIRE_THROWS_WITH(create.run(group, created), "Name expected");
    }

    SECTION("From json, empty conditions")
    {
        {
            std::string json = R"(
                {
                    "name": "By Type",
                }
            )";
            fty::Group group;
            pack::json::deserialize(json, group);

            fty::job::Create create;
            fty::Group created;
            REQUIRE_THROWS_WITH(create.run(group, created), "Any condition is expected");
        }
        {
            std::string json = R"(
                {
                    "name": "By Type",
                    "rules": {
                        "conditions": [
                        ],
                        "operator": "WTF"
                    }
                }
            )";
            fty::Group group;
            pack::json::deserialize(json, group);

            fty::job::Create create;
            fty::Group created;
            REQUIRE_THROWS_WITH(create.run(group, created), "Any condition is expected");
        }
    }

    SECTION("From json, empty value")
    {
        std::string json = R"(
            {
                "name": "By Type",
                "rules": {
                    "conditions": [
                        {
                            "field": "type",
                            "operator": "IS",
                        }
                    ],
                    "operator": "OR"
                }
            }
        )";
        fty::Group group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group created;
        REQUIRE_THROWS_WITH(create.run(group, created), "Value of condition is expected");
    }
}