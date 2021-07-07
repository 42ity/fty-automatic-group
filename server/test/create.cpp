#include "lib/jobs/create.h"
#include "db.h"
#include "lib/storage.h"
#include <catch2/catch.hpp>

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
        fty::Group       created;
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
        fty::Group  group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group       created;
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
        fty::Group  group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group       created;
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
        fty::Group  group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group       created;
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
        fty::Group  group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group       created;
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
            fty::Group  group;
            pack::json::deserialize(json, group);

            fty::job::Create create;
            fty::Group       created;
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
            fty::Group  group;
            pack::json::deserialize(json, group);

            fty::job::Create create;
            fty::Group       created;
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
        fty::Group  group;
        pack::json::deserialize(json, group);

        fty::job::Create create;
        fty::Group       created;
        REQUIRE_THROWS_WITH(create.run(group, created), "Value of condition is expected");
    }


    SECTION("From yaml, group CONTAINS")
    {
        static std::string jaml(R"(
              name  : ByName
              rules : 
                  operator  : AND
                  conditions:
                    - field    : group
                      operator : CONTAINS
                      value    : 1
            )");

        fty::Group group;

        if (auto ret = pack::yaml::deserialize(jaml, group); !ret) {
            FAIL(ret.error());
        }
        fty::job::Create create;
        fty::Group       created;
        REQUIRE_THROWS_WITH(create.run(group, created), "Valid value of condition for linked group is expected");
          }

    CHECK(fty::Storage::clear());

  
    SECTION("Unique name")
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
                    "operator": "OR"
                }
            }
        )";

        fty::Group group;
        pack::json::deserialize(json, group);

        fty::job::Create create;

        fty::Group created;
        REQUIRE_NOTHROW(create.run(group, created));

        fty::Group other;
        REQUIRE_THROWS_WITH(create.run(group, other), "Group with name 'By types' already exists");
    }

    CHECK(fty::Storage::clear());
}
