#include "lib/jobs/resolve.h"
#include "db.h"
#include "lib/jobs/create.h"
#include "lib/jobs/remove.h"
#include "lib/storage.h"
#include <catch2/catch.hpp>

using namespace fmt::literals;

class Group : public fty::Group
{
public:
    Group() = default;

    Group(fty::Group&& other)
        : fty::Group(other)
    {
    }

    ~Group() override
    {
        remove();
    }

    Group create()
    {
        fty::job::Create           create;
        fty::commands::create::Out created;
        REQUIRE_NOTHROW(create.run(*this, created));
        return Group(std::move(created));
    }

    void remove()
    {
        if (id.hasValue()) {
            fty::job::Remove remove;

            fty::commands::remove::In  in;
            fty::commands::remove::Out out;
            in.append(id);
            REQUIRE_NOTHROW(remove.run(in, out));
            REQUIRE(out.size() == 1);
            CHECK(out[0][fty::convert<std::string>(id.value())] == "Ok");
            clear();
        }
    }

    fty::commands::resolve::Out resolve()
    {
        fty::job::Resolve resolve;

        fty::commands::resolve::In  in;
        fty::commands::resolve::Out out;

        in.id = id;
        REQUIRE_NOTHROW(resolve.run(in, out));

        return out;
    }
};

TEST_CASE("Resolve by name with rules input")
{
  auto resolve = [](const std::string & json) -> fty::commands::resolve::Out {
      fty::job::Resolve resolveObj;

      fty::commands::resolve::In  in;
      fty::commands::resolve::Out out;

      if (auto ret = pack::yaml::deserialize(json, in); !ret) {
          throw std::runtime_error(ret.error());
      }

      REQUIRE_NOTHROW(resolveObj.run(in, out));
      return out;
  };

    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv1
                      ext-name : srv1
                    - type     : Server
                      name     : srv2
                      ext-name : srv2
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        //"Contains"
        {
          std::string json = R"(
            {
              "rules": {
                  "operator": "AND",
                  "conditions": [
                    {
                      "field": "name",
                      "operator": "CONTAINS",
                      "value": "srv1"
                    }
                  ]
              }
            }
           )";
            auto info = resolve(json);

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv1");
            CHECK(info[1].name == "srv11");
        }

        //"DoesNotContain"
        {
            std::string json = R"(
              {
                "rules": {
                    "operator": "AND",
                    "conditions": [
                      {
                        "field": "name",
                        "operator": "DOESNOTCONTAIN",
                        "value": "srv1"
                      }
                    ]
                }
              }
            )";

            auto info = resolve(json);

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv2");
            CHECK(info[1].name == "srv21");
        }

        //"Is"
        {
            std::string json = R"(
              {
                "rules": {
                    "operator": "AND",
                    "conditions": [
                      {
                        "field": "name",
                        "operator": "IS",
                        "value": "srv11"
                      }
                    ]
                }
              }
            )";

            auto info = resolve(json);

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        //"IsNot"
        {
            std::string json = R"(
              {
                "rules": {
                    "operator": "AND",
                    "conditions": [
                      {
                        "field": "name",
                        "operator": "ISNOT",
                        "value": "srv11"
                      }
                    ]
                }
              }
            )"; 

            auto info = resolve(json);

            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "srv1");
            CHECK(info[1].name == "srv2");
            CHECK(info[2].name == "srv21");
        }

        //"Not exists"
        { 
            std::string json = R"(
              {
                "rules": {
                    "operator": "AND",
                    "conditions": [
                      {
                        "field": "name",
                        "operator": "IS",
                        "value": "wtf"
                      }
                    ]
                }
              }
            )"; 

            auto info = resolve(json);

            REQUIRE(info.size() == 0);
        } 

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by name")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv1
                      ext-name : srv1
                    - type     : Server
                      name     : srv2
                      ext-name : srv2
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "ByName";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::Name;

        //"Contains"
        {
            cond.value = "srv1";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv1");
            CHECK(info[1].name == "srv11");
        }

        //"DoesNotContain"
        {
            cond.value = "srv1";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv2");
            CHECK(info[1].name == "srv21");
        }

        //"Is"
        {
            cond.value = "srv11";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        //"IsNot"
        {
            cond.value = "srv11";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "srv1");
            CHECK(info[1].name == "srv2");
            CHECK(info[2].name == "srv21");
        }

        //"Not exists"
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by InternalName")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
            )");

        Group group;
        group.name          = "ByInternalName";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::InternalName;

        // Contains
        {
            cond.value = "srv";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
        }

        //"DoesNotContain"
        {
            cond.value = "srv1";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
            CHECK(info[2].name == "srv21");
        }

        // Is
        {
            cond.value = "srv11";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        // IsNot
        {
            cond.value = "srv11";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
            CHECK(info[2].name == "srv21");
        }

        // Not exists
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by location 3 | find vm and hypervisors as well")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Room
                      name     : room
                      ext-name : Room
                      items:
                        - type     : Rack
                          name     : rack
                          ext-name : Rack
                          items:
                            - type     : Server
                              name     : srv11
                              ext-name : srv11
                            - type     : Server
                              name     : srv112
                              ext-name : srv112
                - type : Hypervisor
                  name : hypervisor
                - type : Hypervisor
                  name : hypervisor1
                - type : Hypervisor
                  name : hypervisor2
                - type : Hypervisor
                  name : hypervisor3
                - type : virtual-machine
                  name : vm1
                - type : virtual-machine
                  name : vm2
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            links:
                - dest : hypervisor
                  src  : srv11
                  type : ipminfra.server.hosts.os
                - dest : hypervisor2
                  src  : srv112
                  type : ipminfra.server.hosts.os
                - dest : hypervisor3
                  src  : srv21
                  type : ipminfra.server.hosts.os
                - dest : vm1
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm2
                  src  : hypervisor3
                  type : vmware.esxi.hosts.vm
            )");

        Group group;
        group.name          = "ByLocation";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::Location;

        //"Contains"
        {
            cond.value = "room";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 5);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv112");
            CHECK(info[2].name == "hypervisor");
            CHECK(info[3].name == "hypervisor2");
            CHECK(info[4].name == "vm1");
        }

         // "Is"
        {
            cond.value = "room";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 5);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv112");
            CHECK(info[2].name == "hypervisor");
            CHECK(info[3].name == "hypervisor2");
            CHECK(info[4].name == "vm1");
        }

        //"IsNot"
        {
            cond.value = "room";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();
            REQUIRE(info.size() == 4);
            CHECK(info[0].name == "hypervisor1");
            CHECK(info[1].name == "hypervisor3");
            CHECK(info[2].name == "vm2");
            CHECK(info[3].name == "srv21");
        }

        //"Not exists"
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }


        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by location")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "ByLocation";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::Location;

        //"Contains"
        {
            cond.value = "datacenter";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
        }

        //"DoesNotContain"
        {
            cond.value = "datacenter1";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        // "Is"
        {
            cond.value = "datacenter";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        //"IsNot"
        {
            cond.value = "datacenter";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv21");
        }

        //"Not exists"
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }


        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by location 2")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Room
                      name     : room
                      ext-name : Room
                      items:
                        - type     : Rack
                          name     : rack
                          ext-name : Rack
                          items:
                            - type     : Server
                              name     : srv11
                              ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "ByLocation";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::Location;

        //"Contains"
        {
            cond.value = "room";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        // "Is"
        {
            cond.value = "room";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        //"IsNot"
        {
            cond.value = "room";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv21");
        }

        //"Not exists"
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }


        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by type")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "ByType";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::Type;

        // Contains
        {
            cond.value = "device";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
        }

        //"DoesNotContain"
        {
            cond.value = "device";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
        }


        // Is
        {
            cond.value = "device";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
        }

        // IsNot
        {
            cond.value = "device";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
        }

        /// Not exists
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();
            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by subtype")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "BySubType";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::SubType;

        // Contains
        {
            cond.value = "server";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
        }

        //"DoesNotContain"
        {
            cond.value = "serv";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
        }

        // Is
        {
            cond.value = "server";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
        }

        // IsNot
        {
            cond.value = "server";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
        }

        // Not exists
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();
            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by hostname")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                      attrs:
                          hostname.1 : localhost
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "ByHostName";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::HostName;

        // Contains
        {
            cond.value = "local";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1); // 1 over 4
            CHECK(info[0].name == "srv11");
        }

        //"DoesNotContain"
        {
            cond.value = "local";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3); 
        }

        // Is
        {
            cond.value = "localhost";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        // IsNot
        {
            cond.value = "localhost";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
        }

        // Not exists
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by contact")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      attrs:
                          device.contact : dim
                    - type     : Server
                      name     : srv12
                      attrs:
                          contact_email  : dim@eaton.com
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
            )");

        Group group;
        group.name          = "ByContact";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::Contact;

        // Contains
        {
            cond.value = "dim";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv12");
        }

        //"DoesNotContain"
        {
            cond.value = "dim";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "datacenter1");
            CHECK(info[2].name == "srv21");
        }

        // Is
        {
            cond.value = "dim";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        // Is not
        {
            cond.value = "dim";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 4);
            CHECK(info[0].name == "datacenter");
            CHECK(info[1].name == "srv12");
            CHECK(info[2].name == "datacenter1");
            CHECK(info[3].name == "srv21");
        }

        // Not exists
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by ip address")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      attrs:
                          ip.1 : 127.0.0.1
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      attrs:
                          ip.1 : 192.168.0.1
            )");

        Group group;
        group.name          = "ByIpAddress";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var  = group.rules.conditions.append();
        auto& cond = var.reset<fty::Group::Condition>();
        cond.field = fty::Group::Fields::IPAddress;

        // Contains
        {
            cond.value = "127.0";
            cond.op    = fty::Group::ConditionOp::Contains;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        //"DoesNotContain"
        {
            cond.value = "127.0";
            cond.op    = fty::Group::ConditionOp::DoesNotContain;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
        }

        // Is
        {
            cond.value = "127.0.0.1";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }


        // Is not
        {
            cond.value = "127.0.0.1";
            cond.op    = fty::Group::ConditionOp::IsNot;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
        }

        // Not exists
        {
            cond.value = "wtf";
            cond.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by name and location")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
            )");

        Group group;
        group.name          = "ByName";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        {
            auto& var1  = group.rules.conditions.append();
            auto& cond1 = var1.reset<fty::Group::Condition>();
            cond1.field = fty::Group::Fields::Name;

            auto& var2  = group.rules.conditions.append();
            auto& cond2 = var2.reset<fty::Group::Condition>();
            cond2.field = fty::Group::Fields::Location;
        }

        // Contains
        {
            auto& cond1 = group.rules.conditions[0].get<fty::Group::Condition>();
            cond1.value = "srv";
            cond1.op    = fty::Group::ConditionOp::Contains;

            auto& cond2 = group.rules.conditions[1].get<fty::Group::Condition>();
            cond2.value = "datacenter";
            cond2.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv11");
        }

        // Is
        {
            auto& cond1 = group.rules.conditions[0].get<fty::Group::Condition>();
            cond1.value = "srv21";
            cond1.op    = fty::Group::ConditionOp::Is;

            auto& cond2 = group.rules.conditions[1].get<fty::Group::Condition>();
            cond2.value = "datacenter1";
            cond2.op    = fty::Group::ConditionOp::Is;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "srv21");
        }

        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by name and location/mutlty")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
                    - type     : Server
                      name     : srv22
                      ext-name : srv22
            )");

        Group group;
        group.name          = "ByName";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var1  = group.rules.conditions.append();
        auto& cond1 = var1.reset<fty::Group::Condition>();
        cond1.op    = fty::Group::ConditionOp::Is;
        cond1.value = "datacenter1";
        cond1.field = fty::Group::Fields::Location;

        auto& var2  = group.rules.conditions.append();
        auto& rules = var2.reset<fty::Group::Rules>();
        {
            rules.groupOp = fty::Group::LogicalOp::Or;

            auto& scond1 = rules.conditions.append().reset<fty::Group::Condition>();
            scond1.op    = fty::Group::ConditionOp::Is;
            scond1.field = fty::Group::Fields::Name;
            scond1.value = "srv21";

            auto& scond2 = rules.conditions.append().reset<fty::Group::Condition>();
            scond2.op    = fty::Group::ConditionOp::Is;
            scond2.field = fty::Group::Fields::Name;
            scond2.value = "srv22";
        }

        auto g    = group.create();
        auto info = g.resolve();

        REQUIRE(info.size() == 2);
        CHECK(info[0].name == "srv21");
        CHECK(info[1].name == "srv22");
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by hostname vm")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Connector
                  name : connector
                - type : InfraService
                  name : infra
                - type : Hypervisor
                  name : hypervisor
                - type : Hypervisor
                  name : hypervisor1
                - type : virtual-machine
                  name : vm1
                  attrs:
                      hostName : hypo
                      address  : "[/127.0.0.1,]"
                - type : virtual-machine
                  name : vm2
                - type : virtual-machine
                  name : vm3
                  attrs:
                      hostName : hypo1
                      address  : "[/192.168.0.1,]"
            links:
                - dest : vm1
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm2
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm3
                  src  : hypervisor1
                  type : vmware.esxi.hosts.vm
                - dest : hypervisor
                  src  : infra
                  type : vmware.vcenter.monitors.esxi
                - dest : hypervisor1
                  src  : infra
                  type : vmware.vcenter.monitors.esxi
                - dest : infra
                  src  : connector
                  type : vmware.connected.to.vcenter
            )");

        Group group;
        group.name          = "ByHostNameVm";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var1  = group.rules.conditions.append();
        auto& cond1 = var1.reset<fty::Group::Condition>();

        // Contains
        {
            cond1.op    = fty::Group::ConditionOp::Contains;
            cond1.value = "hypo";
            cond1.field = fty::Group::Fields::HostName;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "vm1");
            CHECK(info[1].name == "vm3");
        }

        //"DoesNotContain"
        {
            cond1.op    = fty::Group::ConditionOp::DoesNotContain;
            cond1.value = "po1";
            cond1.field = fty::Group::Fields::HostName;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 6);
        }

        // Is
        {
            cond1.op    = fty::Group::ConditionOp::Is;
            cond1.value = "hypo";
            cond1.field = fty::Group::Fields::HostName;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
        }

        // Is not
        {
            cond1.op    = fty::Group::ConditionOp::IsNot;
            cond1.value = "hypo1";
            cond1.field = fty::Group::Fields::HostName;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 6);
        }

        // Wrong
        {
            cond1.op    = fty::Group::ConditionOp::Is;
            cond1.value = "wtf";
            cond1.field = fty::Group::Fields::HostName;

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by ip address vm")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Connector
                  name : connector
                - type : InfraService
                  name : infra
                - type : Hypervisor
                  name : hypervisor
                - type : Hypervisor
                  name : hypervisor1
                - type : virtual-machine
                  name : vm1
                  attrs:
                      hostName : hypo
                      ip  : "127.0.0.1"
                - type : virtual-machine
                  name : vm2
                - type : virtual-machine
                  name : vm3
                  attrs:
                      hostName : hypo1
                      ip  : "192.168.0.1"
            links:
                - dest : vm1
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm2
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm3
                  src  : hypervisor1
                  type : vmware.esxi.hosts.vm
                - dest : hypervisor
                  src  : infra
                  type : vmware.vcenter.monitors.esxi
                - dest : hypervisor1
                  src  : infra
                  type : vmware.vcenter.monitors.esxi
                - dest : infra
                  src  : connector
                  type : vmware.connected.to.vcenter
            )");

        Group group;
        group.name          = "ByIpAddressVm";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var1  = group.rules.conditions.append();
        auto& cond1 = var1.reset<fty::Group::Condition>();
        cond1.field = fty::Group::Fields::IPAddress;

        // Contains
        {
            cond1.op    = fty::Group::ConditionOp::Contains;
            cond1.value = "127.0";

            auto g    = group.create();
            auto info = g.resolve();

            //REQUIRE(info.size() == 1);
            //CHECK(info[0].name == "vm1");
            REQUIRE(info.size() == 0); //hot fix to not include VM
        }

        // Contains
        {
            cond1.op    = fty::Group::ConditionOp::Contains;
            cond1.value = "127.*.1";

            auto g    = group.create();
            auto info = g.resolve();

            //REQUIRE(info.size() == 1);
            //CHECK(info[0].name == "vm1");
            REQUIRE(info.size() == 0); //hot fix to not include VM
        }

        //"DoesNotContain"
        {
            cond1.op    = fty::Group::ConditionOp::DoesNotContain;
            cond1.value = "127.0";

            auto g    = group.create();
            auto info = g.resolve();
            //REQUIRE(info.size() == 6); //hot fix to not include VM
            REQUIRE(info.size() == 7);
        }

        // Is
        {
            cond1.op    = fty::Group::ConditionOp::Is;
            cond1.value = "127.0.0.1";

            auto g    = group.create();
            auto info = g.resolve();

            //REQUIRE(info.size() == 1);
            //CHECK(info[0].name == "vm1");
            REQUIRE(info.size() == 0); //hot fix to not include VM
        }

        // Is not
        {
            cond1.op    = fty::Group::ConditionOp::IsNot;
            cond1.value = "127.0.0.1";

            auto g    = group.create();
            auto info = g.resolve();

            //REQUIRE(info.size() == 6); //hot fix to not include VM
            REQUIRE(info.size() == 7);
        }

        // Wrong
        {
            cond1.op    = fty::Group::ConditionOp::Is;
            cond1.value = "wtf";

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by hosted by")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Connector
                  name : connector
                - type : InfraService
                  name : infra
                - type : Hypervisor
                  name : hypervisor
                - type : Hypervisor
                  name : hypervisor1
                - type : virtual-machine
                  name : vm1
                - type : virtual-machine
                  name : vm2
                - type : virtual-machine
                  name : vm3
            links:
                - dest : vm1
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm2
                  src  : hypervisor
                  type : vmware.esxi.hosts.vm
                - dest : vm3
                  src  : hypervisor1
                  type : vmware.esxi.hosts.vm
                - dest : hypervisor
                  src  : infra
                  type : vmware.vcenter.monitors.esxi
                - dest : hypervisor1
                  src  : infra
                  type : vmware.vcenter.monitors.esxi
                - dest : infra
                  src  : connector
                  type : vmware.connected.to.vcenter
            )");

        Group group;
        group.name          = "ByHostedBy";
        group.rules.groupOp = fty::Group::LogicalOp::And;

        auto& var1  = group.rules.conditions.append();
        auto& cond1 = var1.reset<fty::Group::Condition>();
        cond1.field = fty::Group::Fields::HostedBy;

        // Contains
        {
            cond1.op    = fty::Group::ConditionOp::Contains;
            cond1.value = "hypervisor";

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "vm1");
            CHECK(info[1].name == "vm2");
            CHECK(info[2].name == "vm3");
        }

        // DoesNotContain
        {
            cond1.op    = fty::Group::ConditionOp::DoesNotContain;
            cond1.value = "visor1";

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "vm1");
            CHECK(info[1].name == "vm2");
        }

        // Is
        {
            cond1.op    = fty::Group::ConditionOp::Is;
            cond1.value = "hypervisor";

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 2);
            CHECK(info[0].name == "vm1");
            CHECK(info[1].name == "vm2");
        }

        // Is not
        {
            cond1.op    = fty::Group::ConditionOp::IsNot;
            cond1.value = "hypervisor";

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "vm3");
        }

        // Wrong
        {
            cond1.op    = fty::Group::ConditionOp::Is;
            cond1.value = "wtf";

            auto g    = group.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 0);
        }

    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}

TEST_CASE("Resolve by Group")
{
    try {
        fty::SampleDb db(R"(
            items:
                - type : Datacenter
                  name : datacenter
                  items:
                    - type     : Server
                      name     : srv11
                      ext-name : srv11
                - type : Datacenter
                  name : datacenter1
                  items:
                    - type     : Server
                      name     : srv21
                      ext-name : srv21
                - type : Datacenter
                  name : datacenter2
                  items:
                    - type     : Server
                      name     : serv
                      ext-name : serv
                    - type     : Server
                      name     : servsrv
                      ext-name : servsrv
            )");

        static std::string groupNameJaml(R"(
              name  : ByName
              rules : 
                  operator  : AND
                  conditions:
                    - field    : name
                      operator : CONTAINS
                      value    : srv
            )");

        Group groupName;

        if (auto ret = pack::yaml::deserialize(groupNameJaml, groupName); !ret) {
            FAIL(ret.error());
        }

        auto createdName = groupName.create();

        // And operator | Contains
        {
            std::string groupWithLinkJaml = R"(
                name  : WithLink
                rules :
                    operator  : AND
                    conditions:
                      - field    : name
                        operator : CONTAINS
                        value    : serv
                      - field    : group
                        operator : IS
                        value    : {}
            )"_format(createdName.id.value());

            Group groupLink;

            if (auto ret = pack::yaml::deserialize(groupWithLinkJaml, groupLink); !ret) {
                FAIL(ret.error());
            }
            auto g    = groupLink.create();
            auto info = g.resolve();
            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "servsrv");
        }

        // IsNot in group
        {
            std::string groupWithLinkJaml = R"(
                name  : WithLink
                rules :
                    operator  : AND
                    conditions:
                      - field    : name
                        operator : ISNOT
                        value    : serv
                      - field    : group
                        operator : IS
                        value    : {}
            )"_format(createdName.id.value());

            Group groupLink;

            if (auto ret = pack::yaml::deserialize(groupWithLinkJaml, groupLink); !ret) {
                FAIL(ret.error());
            }

            auto g    = groupLink.create();
            auto info = g.resolve();
            REQUIRE(info.size() == 3);
            CHECK(info[0].name == "srv11");
            CHECK(info[1].name == "srv21");
            CHECK(info[2].name == "servsrv");
        }

        // IsNot group
        {
            std::string groupWithLinkJaml = R"(
                name  : WithLink
                rules :
                    operator  : AND
                    conditions:
                      - field    : name
                        operator : CONTAINS
                        value    : serv
                      - field    : group
                        operator : ISNOT
                        value    : {}
            )"_format(createdName.id.value());

            Group groupLink;

            if (auto ret = pack::yaml::deserialize(groupWithLinkJaml, groupLink); !ret) {
                FAIL(ret.error());
            }

            auto g    = groupLink.create();
            auto info = g.resolve();
            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "serv");
        }

        // IsNot with two linked groups
        {
            std::string groupTmpJaml(R"(
                name  : LinkTmp
                rules : 
                    operator  : AND
                    conditions:
                      - field    : name
                        operator : CONTAINS
                        value    : srv21
            )");

            Group groupLink;
            Group groupTmp;

            if (auto ret = pack::yaml::deserialize(groupTmpJaml, groupTmp); !ret) {
                FAIL(ret.error());
            }

            auto gTmp = groupTmp.create();

            std::string groupWithLinkJaml = R"(
                name : WithLink
                rules :
                    operator : AND
                    conditions:
                      - field : name
                        operator : CONTAINS
                        value : s
                      - field : group
                        operator : ISNOT
                        value : {}
                      - field : group
                        operator : ISNOT
                        value : {}
            )"_format(createdName.id.value(), gTmp.id.value());

            if (auto ret = pack::yaml::deserialize(groupWithLinkJaml, groupLink); !ret) {
                FAIL(ret.error());
            }

            auto g    = groupLink.create();
            auto info = g.resolve();

            REQUIRE(info.size() == 1);
            CHECK(info[0].name == "serv");
        }

        createdName.remove();
        CHECK(fty::Storage::clear());
    } catch (const std::exception& ex) {
        FAIL(ex.what());
    }
}
