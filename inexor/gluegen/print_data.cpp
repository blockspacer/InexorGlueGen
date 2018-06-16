
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/gluegen/SharedAttributes.hpp"
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

/// Add templatedata for this shared variable coming from shared attributes.
void add_attributes_templatedata(mustache::data &variable_data,
                                 vector<SharedVariable::attached_attribute> attached_attributes,
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
    for(const SharedVariable::attached_attribute &attribute : attached_attributes)
    {
        if(attribute_definitions.find(attribute.name) == attribute_definitions.end()) {
            // std::cout << "node: " << node.get_name_cpp_full() << " attribute: " << node_attribute.name << std::endl;
            // this argument is not corresponding with the name of a sharedattribute.
            continue;
        }

        const attribute_definition def = attribute_definitions.find(attribute.name)->second;

        if(def.constructor_args.size() < attribute.constructor_args.size()) {
            // more arguments used for the instance than the declaration allows.
            std::cout << "Warn: Too much constructor arguments given for " << def.name << std::endl;
        }

        mustache::data constructor_args_data{mustache::data::type::list};
        for(size_t i = 0; i < def.constructor_args.size(); i++)
        {
            const name_defaultvalue_tupel &def_construct_param = def.constructor_args[i];
            const string param_name = def_construct_param.name;


            mustache::data arg_data{mustache::data::type::object};
            arg_data.set("attr_arg_name", param_name);

            if (def_construct_param.default_value.empty()){
                if (i >= attribute.constructor_args.size()) {
                    std::cout << "Error: Not enough constructor arguments given for " << def.name << std::endl;
                    break;
                }
                arg_data.set("attr_arg_value", attribute.constructor_args[i]);
            }
            else
            {
                const string param_value_template = def_construct_param.default_value;
                mustache::mustache tmpl{param_value_template};
                arg_data.set("attr_arg_value", tmpl.render(variable_data));
            }
            constructor_args_data.push_back(arg_data);
        }

        mustache::data attribute_data{mustache::data::type::object};
        attribute_data.set("attr_name", attribute.name);
        attribute_data.set("attr_constructor_args", constructor_args_data);
        attached_attributes_data.push_back(attribute_data);
    }
    variable_data.set("attached_attributes", attached_attributes_data);
}

/// Print a type with template arguments from a type_node_t.
/// Output e.g. "sharedvar<int, int>"
/// or "sharedvar-int_int-" when non-default separators are given.
const string print_full_type(const SharedVariable::type_node_t &type,
                            const unordered_map<string, shared_class_definition> &type_definitions,
                            const string template_open = "<",
                            const string template_seperator = ", ",
                            const string template_close = ">")
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
        if (i == 0) buf += template_open;
        buf += print_full_type(type.template_types[i], type_definitions,
                               template_open, template_seperator, template_close);
        if (i==type.template_types.size()-1) buf += template_close;
        else buf += template_seperator;
    }
    return buf;
}

/// Add all template data entries corresponding to the type information of the variable.
void add_type_node_data(const SharedVariable::type_node_t &type_node,
                        const unordered_map<string, shared_class_definition> &type_definitions,
                        mustache::data &data)
{
    if(type_definitions.count(type_node.uniqueID()))
    {
        const auto &classdef = type_definitions.find(type_node.uniqueID())->second;
        data.set("is_" + classdef.class_name, mustache::data::type::bool_true);
    } else {
        // its not a class, which was found, but either an unresolved class type (without refid) or a builtin type
        // (int, float..)
        data.set("is_" + var.type.refid, mustache::data::type::bool_true);
        data.set("is_builtin_type", mustache::data::type::bool_true);
    }

    data.set("type_name_cpp", print_full_type(type_node, type_definitions));
    data.set("type_name_unique", print_full_type(type_node, type_definitions, "__", "_", "__"));
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
