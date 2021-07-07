#include "common/group.h"

namespace fty {

std::ostream& operator<<(std::ostream& ss, fty::Group::ConditionOp value)
{
    ss << [&]() {
        switch (value) {
            case fty::Group::ConditionOp::Contains:
                return "CONTAINS";
            case fty::Group::ConditionOp::DoesNotContain:
                return "DOESNOTCONTAIN";
            case fty::Group::ConditionOp::Is:
                return "IS";
            case fty::Group::ConditionOp::IsNot:
                return "ISNOT";
            case fty::Group::ConditionOp::Unknown:
                return "UNKNOWN";
        }
        return "Unknown";
    }();
    return ss;
}

std::istream& operator>>(std::istream& ss, fty::Group::ConditionOp& value)
{
    std::string strval;
    ss >> strval;
    if (strval == "CONTAINS") {
        value = fty::Group::ConditionOp::Contains;
    } else if (strval == "IS") {
        value = fty::Group::ConditionOp::Is;
    } else if (strval == "DOESNOTCONTAIN") {
        value = fty::Group::ConditionOp::DoesNotContain;
    } else if (strval == "ISNOT") {
        value = fty::Group::ConditionOp::IsNot;
    } else {
        value = fty::Group::ConditionOp::Unknown;
    }
    return ss;
}

std::ostream& operator<<(std::ostream& ss, fty::Group::LogicalOp value)
{
    ss << [&]() {
        switch (value) {
            case fty::Group::LogicalOp::And:
                return "AND";
            case fty::Group::LogicalOp::Or:
                return "OR";
            case fty::Group::LogicalOp::Unknown:
                return "Unknown";
        }
        return "n";
    }();
    return ss;
}

std::istream& operator>>(std::istream& ss, fty::Group::LogicalOp& value)
{
    std::string strval;
    ss >> strval;
    if (strval == "AND") {
        value = fty::Group::LogicalOp::And;
    } else if (strval == "OR") {
        value = fty::Group::LogicalOp::Or;
    } else {
        value = fty::Group::LogicalOp::Unknown;
    }
    return ss;
}

std::ostream& operator<<(std::ostream& ss, fty::Group::Fields value)
{
    ss << [&]() {
        switch (value) {
            case fty::Group::Fields::Contact:
                return "contact";
            case fty::Group::Fields::HostName:
                return "host-name";
            case fty::Group::Fields::IPAddress:
                return "ip-address";
            case fty::Group::Fields::Location:
                return "location";
            case fty::Group::Fields::Name:
                return "name";
            case fty::Group::Fields::SubType:
                return "subtype";
            case fty::Group::Fields::Type:
                return "type";
            case fty::Group::Fields::InternalName:
                return "asset";
            case fty::Group::Fields::HostedBy:
                return "hosted-by";
            case fty::Group::Fields::Group:
                return "group";
            case fty::Group::Fields::Unknown:
                return "unknown";
        }
        return "unknown";
    }();
    return ss;
}

std::istream& operator>>(std::istream& ss, fty::Group::Fields& value)
{
    value = fty::Group::Fields::Unknown;
    std::string strval;
    ss >> strval;
    if (strval == "unknown") {
        value = fty::Group::Fields::Unknown;
    } else if (strval == "contact") {
        value = fty::Group::Fields::Contact;
    } else if (strval == "host-name") {
        value = fty::Group::Fields::HostName;
    } else if (strval == "ip-address") {
        value = fty::Group::Fields::IPAddress;
    } else if (strval == "location") {
        value = fty::Group::Fields::Location;
    } else if (strval == "name") {
        value = fty::Group::Fields::Name;
    } else if (strval == "type") {
        value = fty::Group::Fields::Type;
    } else if (strval == "subtype") {
        value = fty::Group::Fields::SubType;
    } else if (strval == "asset") {
        value = fty::Group::Fields::InternalName;
    } else if (strval == "hosted-by") {
        value = fty::Group::Fields::HostedBy;
    } else if (strval == "group") {
        value = fty::Group::Fields::Group;
    }
    return ss;
}

static Expected<void, fty::Translate> checkRules(const Group::Rules& rule)
{
    if (!rule.conditions.size()) {
        return unexpected("Any condition is expected"_tr);
    }

    if (rule.groupOp == Group::LogicalOp::Unknown) {
        return unexpected("Valid logical operator is expected"_tr);
    }

    for (const auto& cond : rule.conditions) {
        if (cond.is<Group::Condition>()) {
            const auto& val = cond.get<Group::Condition>();
            if (val.field == Group::Fields::Unknown) {
                return unexpected("Valid field name is expected"_tr);
            }
            if (val.op == Group::ConditionOp::Unknown) {
                return unexpected("Valid condition operator is expected"_tr);
            }
            if (!val.value.hasValue()) {
                return unexpected("Value of condition is expected"_tr);
            }
            if (val.field == Group::Fields::Group && val.op != fty::Group::ConditionOp::IsNot && val.op != fty::Group::ConditionOp::Is) {
                return unexpected("Valid value of condition for linked group is expected"_tr);
            }
        } else if (cond.is<Group::Rules>()) {
            if (auto ret = checkRules(cond.get<Group::Rules>()); !ret) {
                return unexpected(ret.error());
            }
        }
    }

    return {};
}

Expected<void, fty::Translate> Group::check() const
{
    if (!hasValue()) {
        return unexpected("Group is empty"_tr);
    }

    if (!name.hasValue()) {
        return unexpected("Name expected"_tr);
    }

    if (auto ret = checkRules(rules); !ret) {
        return unexpected(ret.error());
    }

    return {};
}

} // namespace fty
