#pragma once
#include <pack/pack.h>
#include <fty/translate.h>

namespace fty {

struct Group : public pack::Node
{
    enum class LogicalOp
    {
        Unknown,
        Or,
        And
    };

    enum class ConditionOp
    {
        Unknown,
        Contains,
        Is,
        IsNot,
        DoesNotContain
    };

    enum class Fields
    {
        Unknown,
        Name,
        Location,
        Type,
        SubType,
        Contact,
        HostName,
        IPAddress,
        InternalName,
        HostedBy,
        Group
    };

    struct Condition : public pack::Node
    {
        pack::Enum<Fields>      field = FIELD("field");
        pack::Enum<ConditionOp> op    = FIELD("operator");
        pack::String            value = FIELD("value");

        using pack::Node::Node;
        META(Condition, field, op, value);
    };

    struct Rules : public pack::Node
    {
        pack::Enum<LogicalOp>                             groupOp    = FIELD("operator");
        pack::ObjectList<pack::Variant<Rules, Condition>> conditions = FIELD("conditions");

        using pack::Node::Node;
        META(Rules, groupOp, conditions);
    };

    pack::UInt64 id    = FIELD("id");
    pack::String name  = FIELD("name");
    Rules        rules = FIELD("rules");

    using pack::Node::Node;
    META(Group, id, name, rules);

    Expected<void, fty::Translate> check() const;
};

std::ostream& operator<<(std::ostream& ss, fty::Group::ConditionOp value);
std::istream& operator>>(std::istream& ss, fty::Group::ConditionOp& value);
std::ostream& operator<<(std::ostream& ss, fty::Group::LogicalOp value);
std::istream& operator>>(std::istream& ss, fty::Group::LogicalOp& value);
std::ostream& operator<<(std::ostream& ss, fty::Group::Fields value);
std::istream& operator>>(std::istream& ss, fty::Group::Fields& value);

} // namespace fty
