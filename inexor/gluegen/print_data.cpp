
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/gluegen/SharedAttributes.hpp"
#include "inexor/filesystem/path.hpp"

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <kainjow/mustache.hpp>

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <set>

using namespace pugi;
using namespace kainjow;
using namespace std;
using boost::algorithm::trim;
using boost::replace_all;

namespace inexor {
namespace gluegen {

/// Add templatedata for this shared variable coming from attributes.
/// Attached atributes are technically class instances.
/// If a defined attribute has default values, the attribute should be added to each variable
/// not having the attribute attached.
/// Also the default value gets used when the attached attribute does pass all possible constructor parameters
/// (so the remaining have the default value set)
void add_attributes_templatedata(mustache::data &variable_data,
                                 const unordered_map<string, SharedVariable::attached_attribute> attached_attributes,
                                 const unordered_map<string, attribute_definition> &attribute_definitions)
{
    /*
     * Flow:
     * 1. add constructor arguments of attribute to local template data
     *
     *      a) if argument given, map it and add it to variable template data
     *              - argument_name: passed value
     *              - dont evaluate passed value (no constant reformatting or w/e currently)
     *      b) if argument not given, but has defaultvalue, use default value
     *              - default value can use the type of the parameter (i.e. int wanted, int given)
     *              - default value can be a string casted to the type of the parameter (i.e. int wanted, string given)
     *                  - in this case, the string gets interpreted as template and rendered with all templatedata which
     *                    is known from the variable (i.e. its type, name, ..)
     *
     * // Later: 2. each "static const char*" member gets added to vars templatedata as well
     *      ----------
     *
     * usage in template:
     *      variable
     *          for each {{attached attribute}}:
     *              for each {{constructor argument}}:
     *                  print "<argument_name>": "<argument_value>"
     */

    // add template data from constructor arguments:
    mustache::data attached_attributes_data{mustache::data::type::list};
    for(auto &deftupel : attribute_definitions)
    {
        auto &def = deftupel.second;
        mustache::data constructor_args_data{mustache::data::type::list};
        // This defined attribute was not found in the list of attached attributes
        bool attribute_not_attached = false;
        bool use_default_value = false;

        auto attached_attr_iter = attached_attributes.find(def.name);
        if(attached_attr_iter == attached_attributes.end())
        {
            if (!def.constructor_has_default_values)
                continue;
            attribute_not_attached = true;
            use_default_value = true;
        }

        for(size_t i = 0; i < def.constructor_args.size(); i++)
        {
            const name_defaultvalue_tupel &def_construct_param = def.constructor_args[i];
            const string param_name = def_construct_param.name;


            mustache::data arg_data{mustache::data::type::object};
            arg_data.set("attr_arg_name", param_name);
            string param_value;


            if (!attribute_not_attached)
            {
                const SharedVariable::attached_attribute attribute = attached_attr_iter->second;

                if (i >= attribute.constructor_args.size())
                {
                    if (def_construct_param.default_value.empty()) {
                        // TODO: Do we handle = "" correctly?
                        std::cout << "Error: Not enough constructor arguments given for " << def.name << std::endl;
                        break;
                    }
                    // incomplete list: take the first params from the caller, the rest is default.
                    use_default_value = true;
                }
            }

            if (use_default_value) {
             //   if (def_construct_param.default_value.empty())
             //       continue;
                const string param_value_template = def_construct_param.default_value;
                mustache::mustache tmpl{param_value_template};
                param_value = tmpl.render(variable_data);
            }
            else {
                param_value = attached_attr_iter->second.constructor_args[i];
            }

            arg_data.set("attr_arg_value", param_value);
            constructor_args_data.push_back(arg_data);
        }

        mustache::data attribute_data{mustache::data::type::object};
        attribute_data.set("attr_name", def.name);
        attribute_data.set("attr_constructor_args", constructor_args_data);
        attached_attributes_data.push_back(attribute_data);
    }
    variable_data.set("attached_attributes", attached_attributes_data);
}

string make_pure_type_printable(const string &pure_type)
{
    string t = pure_type;
    replace_all(t, "<", "_");
    replace_all(t, ">", "_");
    replace_all(t, "*", "p");
    replace_all(t, ":", "_");
    replace_all(t, " ", "_");
    return std::move(t);
}

/// Print a type with template arguments from a type_node_t.
/// Output e.g. "sharedvar<int, int>"
/// or "sharedvar-int_int-" when non-default separators are given.
const string print_full_type(const SharedVariable::type_node_t &type,
                            const unordered_map<string, shared_class_definition> &type_definitions,
                            const string template_open = "<",
                            const string template_seperator = ", ",
                            const string template_close = ">",
                            bool make_printable = false)
{
    std::string buf;

    // either add the class name (if we can resolve it) or the class_name
    if (type_definitions.count(type.uniqueID()))
    {
        buf = type_definitions.find(type.uniqueID())->second.class_name;
    } else {
        buf = make_printable ? make_pure_type_printable(type.pure_type) : type.pure_type;
    }

    for (size_t i = 0; i < type.template_types.size(); i++)
    {
        if (i == 0) buf += template_open;
        buf += print_full_type(type.template_types[i], type_definitions,
                               template_open, template_seperator, template_close, make_printable);
        if (i==type.template_types.size()-1) buf += template_close;
        else buf += template_seperator;
    }
    return buf;
}

/// Each variable's templatedata has a "is_typeint" entry.
/// Since member variables should not have the parents entry, we need to collect all previous definitions and null them
/// explicitely.
void add_is_type_member(const SharedVariable::type_node_t &type_node,
                        const unordered_map<string, shared_class_definition> &type_definitions,
                        mustache::data &data)
{
    string this_classes_ident;
    if(type_definitions.count(type_node.uniqueID()))
    {
        const auto &classdef = type_definitions.find(type_node.uniqueID())->second;
        this_classes_ident = classdef.class_name;
    } else {
        // its not a class, which was found, but either an unresolved class type (without refid) or a builtin type
        // (int, float..)
        this_classes_ident = make_pure_type_printable(type_node.pure_type);
        data.set("is_builtin_type", mustache::data::type::bool_true);
    }

    static set<string> all_entries;
    for (const auto &a : all_entries)
    {
        if (a == this_classes_ident)
            data.set("is_" + a, mustache::data::type::bool_true);
        else
            data.set("is_" + a, mustache::data::type::bool_false);
    }
    all_entries.insert(this_classes_ident);
}

/// Add all template data entries corresponding to the type information of the variable.
void add_type_node_data(const SharedVariable::type_node_t &type_node,
                        const unordered_map<string, shared_class_definition> &type_definitions,
                        mustache::data &data)
{
    add_is_type_member(type_node, type_definitions, data);
    data.set("type_name_cpp", print_full_type(type_node, type_definitions));
    data.set("type_name_unique", print_full_type(type_node, type_definitions, "__", "_", "__", true));

    mustache::data tmpl_data{mustache::data::type::list};
    for (const auto &t : type_node.template_types)
    {
        mustache::data t_data{mustache::data::type::object};
        add_type_node_data(t, type_definitions, t_data);
        tmpl_data.push_back(t_data);
    }
    data.set("template_types", tmpl_data);
}

/// Print all data corresponding to a specific shared variable, set an index for each.
mustache::data get_shared_var_templatedata(const SharedVariable &var,
                                           const unordered_map<string, shared_class_definition> &type_definitions,
                                           const unordered_map<string, attribute_definition> &attribute_definitions,
                                           size_t index)
{
    mustache::data curvariable{mustache::data::type::object};
    add_type_node_data(var.type, type_definitions, curvariable);
    //if(local_index>0) curvariable.set("local_index", std::to_string(local_index));

    mustache::data ns{mustache::data::type::list};
    for(const string &ns_part : var.var_namespace)
        ns.push_back(mustache::data(ns_part));
    curvariable.set("namespace", ns);
    curvariable.set("name", var.name);
    curvariable.set("index", to_string(index));

    add_attributes_templatedata(curvariable, var.attached_attributes, attribute_definitions);

    return curvariable;
}

kainjow::mustache::data print_shared_var_occurences(const vector<SharedVariable> &shared_var_occurences,
                                                    const unordered_map<string, shared_class_definition> &type_definitions,
                                                    const unordered_map<string, attribute_definition> &attribute_definitions)
{
    int index = 21;
    mustache::data sharedvars{mustache::data::type::list};

    for(const auto &shared_var : shared_var_occurences)
    {
        sharedvars.push_back(get_shared_var_templatedata(shared_var, type_definitions, attribute_definitions, index++));
    }
    return sharedvars;
}

/// Create a shared class definition which the
mustache::data get_shared_class_templatedata(const shared_class_definition &def,
                                             const unordered_map<string, shared_class_definition> &type_definitions,
                                             const unordered_map<string, attribute_definition> &attribute_definitions)
{
    mustache::data cur_definition{mustache::data::type::object};
    // The class needs to be defined in a cleanly includeable header file.
    cur_definition.set("header", def.definition_header);

    add_type_node_data(def.type_node, type_definitions, cur_definition);

    mustache::data members{mustache::data::type::list};

    int local_index = 2;
    for(const SharedVariable &child : def.elements)
    {
        members.push_back(get_shared_var_templatedata(child, type_definitions, attribute_definitions, local_index++));
    }
    cur_definition.set("members", members);
    return cur_definition;
}

mustache::data print_type_definitions(const unordered_map<string, shared_class_definition> &type_definitions,
                                      const unordered_map<string, attribute_definition> &attribute_definitions)
{
    mustache::data sharedclasses{mustache::data::type::list};

    for(const auto &class_def : type_definitions)
    {
        sharedclasses.push_back(get_shared_class_templatedata(class_def.second, type_definitions, attribute_definitions));
    }
    return sharedclasses;

}

mustache::data print_data(const vector<SharedVariable> &var_occurences,
                          const unordered_map<string, shared_class_definition> &type_definitions,
                          const unordered_map<string, attribute_definition> &attribute_definitions)
{
    mustache::data data{mustache::data::type::object};
    data.set("type_definitions", print_type_definitions(type_definitions, attribute_definitions));
    data.set("variables", print_shared_var_occurences(var_occurences, type_definitions, attribute_definitions));

    data.set("file_comment", "// This file gets generated!\n"
            "// Do not modify it directly but its corresponding template file instead!");
    return data;

}
// TODO: classes defined in classes won't have the right type.

} } // ns inexor::gluegen
