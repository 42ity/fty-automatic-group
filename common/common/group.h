/*  ====================================================================================================================
    group.h - Definition of the dto group for automatic group

    Copyright (C) 2024 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ====================================================================================================================
*/

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
        And,
    };

    enum class ConditionOp
    {
        Unknown,
        Contains,
        DoesNotContain,
        Is,
        IsNot,
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
        Group,
        Tag,
        Criticality,
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
