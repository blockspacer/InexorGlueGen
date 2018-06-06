
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/filesystem/path.hpp"

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <kainjow/mustache.hpp>

#include <fstream>

using namespace pugi;
using namespace kainjow;
using namespace std;

namespace inexor {
namespace gluegen {
/// Create a shared class definition which the
/// \param def
/// \param ctx
/// \param add_instances
/// \return
mustache::data get_shared_class_templatedata(const shared_class_definition &def,
                                             const unordered_map<string, shared_class_definition> &type_definitions)
{
    mustache::data cur_definition{mustache::data::type::object};
    // The class needs to be defined in a cleanly includeable header file.
    cur_definition.set("header", def.definition_header);

    cur_definition.set("name", def.class_name);
    mustache::data members{mustache::data::type::list};

    int local_index = 2;
    for(const SharedVariable &child : def.elements)
    {
    //    members.push_back(get_shared_var_templatedata(child, type_definitions, local_index++));
    }
    cur_definition.set("members", members);
    return cur_definition;
}

mustache::data print_type_definitions(const unordered_map<string, shared_class_definition> &type_definitions)
{
    mustache::data sharedclasses{mustache::data::type::list};

    for(const auto &class_def : type_definitions)
    {
        sharedclasses.push_back(get_shared_class_templatedata(class_def.second, type_definitions));
    }
    return sharedclasses;

}

const string print_cpp_type(const SharedVariable::type_node_t &type,
                      const unordered_map<string, shared_class_definition> &type_definitions)
{
    std::string buf;

    // either add the class name (if we can resolve it) or the class_name
    if (type_definitions.count(type.uniqueID()))
    {
        buf = type_definitions.find(type.uniqueID())->second.class_name;
    } else {
        buf = type.refid;
    }

    for (size_t i = 0; i < type.template_types.size(); i++)
    {
        if (i == 0) buf += "<";
        buf += print_cpp_type(*type.template_types[i], type_definitions);
        if (i==type.template_types.size()-1) buf += ">";
        else buf += ",";
    }
    return buf;
}

/// Print all data corresponding to a specific shared variable, set an index for each.
mustache::data get_shared_var_templatedata(const SharedVariable &var,
                                           const unordered_map<string, shared_class_definition> &type_definitions,
                                           size_t index)
{
    mustache::data curvariable{mustache::data::type::object};
    if(type_definitions.count(var.type->uniqueID()))
    {
        const auto &classdef = type_definitions.find(var.type->uniqueID())->second;
        curvariable.set("is_" + classdef.class_name, mustache::data::type::bool_true);
    } else {
        // its not a class, which was found, but either an unresolved class type (without refid) or a builtin type
        // (int, float..)
        curvariable.set("is_" + var.type->refid, mustache::data::type::bool_true);
    }

    curvariable.set("type_cpp", print_cpp_type(*var.type, type_definitions));
    //if(local_index>0) curvariable.set("local_index", std::to_string(local_index));

    mustache::data ns{mustache::data::type::list};
    for(const string &ns_part : var.var_namespace)
        ns.push_back(mustache::data(ns_part));
    curvariable.set("namespace", ns);
    curvariable.set("name", var.name);
    curvariable.set("index", to_string(index));

    // add_options_templatedata(curvariable, node.attached_options, data);
    return curvariable;
}

kainjow::mustache::data print_shared_var_occurences(const std::vector<SharedVariable> &shared_var_occurences,
                                        const unordered_map<string, shared_class_definition> &type_definitions)
{
    int index = 21;
    mustache::data sharedvars{mustache::data::type::list};

    for(const auto &shared_var : shared_var_occurences)
    {
        sharedvars.push_back(get_shared_var_templatedata(shared_var, type_definitions, index++));
    }
    return sharedvars;
}
} } // ns inexor::gluegen
