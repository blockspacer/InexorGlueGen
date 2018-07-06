
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/gluegen/SharedAttributes.hpp"
#include "inexor/gluegen/parse_helpers.hpp"
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

enum TYPE {TYPE_INT, TYPE_FLOAT, TYPE_DOUBLE, TYPE_BOOL, TYPE_CHAR_ARRAY};


void remove_substrs(string &str, const string &pattern) {
    string::size_type n = pattern.length();
    for (string::size_type i = str.find(pattern); i != string::npos; i = str.find(pattern))
        str.erase(i, n);
}

void cut_qualifiers(string &literal_type)
{
    remove_substrs(literal_type, "const ");
    // This is not bulletproof, but good enough.
}

bool is_compatible_type(TYPE given_type, string expected_type)
{
    cut_qualifiers(expected_type);
    trim(expected_type);
    switch (given_type) {
        case TYPE_BOOL:{
            if(expected_type == "bool") return true;
        }
        case TYPE_INT:{
            if(expected_type == "int") return true;
        }
        case TYPE_FLOAT:{
            if(expected_type == "float") return true;
        }
        case TYPE_DOUBLE:{
            if(expected_type == "double") return true;
            break;
        }
        case TYPE_CHAR_ARRAY:{
            if(expected_type == "char *") return true;
            if(expected_type == "char*") return true;
            if(expected_type == "std::string") return true;
        }
    }
    return false;
}

/// Returns the primitive type of the literal.
/// Limited in functionality as it only distinquishes between int and char *
const TYPE get_type_of_literal(const string &literal)
{
    if (literal.find('"') != string::npos) return TYPE_CHAR_ARRAY;
    if (literal.find('f') != string::npos) return TYPE_FLOAT;
    if (literal.find('.') != string::npos) return TYPE_DOUBLE;
    if (literal == "true" || literal == "false") return TYPE_BOOL;
    return TYPE_INT;
}

bool is_compatible_constructor(const vector<string> &given_arguments,
                               const attribute_definition::constructor &constr_definition)
{
    int i;
    for(i = 0; i < given_arguments.size(); i++)
    {
        if(i >= constr_definition.constructor_args.size()) return false;

        const function_parameter def_arg = constr_definition.constructor_args[i];
        TYPE t = get_type_of_literal(given_arguments[i]);
        if(!is_compatible_type(t, def_arg.type)) return false;
    }
    // if all arguments until here are compatible, we still didn't handle the case
    // that we did not pass all arguments, but only a subset.
    // in that case, the remaining args need to have default values set.
    size_t expected_arg_number = constr_definition.constructor_args.size();
    if (expected_arg_number != 0 && i < expected_arg_number-1)
        if (!constr_definition.constructor_args[i].default_value.empty())
            return false;
    return true;
}

/// Returns that operator of the list of overloaded operators, which corresponds to the list of given arguments.
const attribute_definition::constructor
    find_constructor_by_typelist(const vector<string> &given_arguments,
                                 const vector<attribute_definition::constructor> &possible_constructors)
{
    for(const attribute_definition::constructor &con : possible_constructors)
    {
        if (is_compatible_constructor(given_arguments, con))
            return con;
    }
    throw std::logic_error("No fitting constructor found");
}

/// This removes whitespace, trim any surrounding whitespace, in the future also convert literals (i.e hex -> decimal)
string convert_literal_to_protobuf_comatible_literal(const string &mangled_literal)
{
    string out = mangled_literal;
    trim(out);
    remove_surrounding_quotes(out);
    return out;
}

/// Add templatedata for this shared variable coming from attributes.
/// Attached atributes are technically class instances.
/// If a defined attribute has default values, the attribute should be added to each variable
/// not having the attribute attached.
/// Also the default value gets used when the attached attribute does pass all possible constructor parameters
/// (so the remaining have the default value set)
void add_attached_attributes_templatedata(mustache::data &variable_data,
                                 const unordered_map<string, SharedVariable::attached_attribute> attached_attributes,
                                 const unordered_map<string, attribute_definition> &attribute_definitions)
{
    /*
     * Flow:
     * 1. add constructor arguments of attribute to local template data
     *
     *      a) if argument given, map it and add it to variable template data
     *              - argument_name: passed value
     *              - dont evaluate passed value (no constant folding or w/e currently)
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

        // This defined attribute was found in the list of attached attributes
        bool attribute_is_attached;
        auto attached_attr_iter = attached_attributes.find(def.name);
        if(attached_attr_iter != attached_attributes.end())
            attribute_is_attached = true;
        else
            attribute_is_attached = false;

        const auto constructor = find_constructor_by_typelist(attribute_is_attached
                                     ? attached_attr_iter->second.constructor_args
                                     : vector<string>(),
                                    def.constructors);

        for(size_t i = 0; i < constructor.constructor_args.size(); i++)
        {
            const function_parameter &def_construct_param = constructor.constructor_args[i];
            const string param_name = def_construct_param.name;


            mustache::data arg_data{mustache::data::type::object};
            arg_data.set("attr_arg_name", param_name);
            string param_value;

            string given_argument = attribute_is_attached ?
                                    (attached_attr_iter->second.constructor_args.size() > i ?
                                        attached_attr_iter->second.constructor_args[i]
                                     : "")
                                    : "";

            if (given_argument.empty()) {
                // use default value.

             //   if (def_construct_param.default_value.empty())
             //       continue;
                const string param_value_template = def_construct_param.default_value;
                mustache::mustache tmpl{param_value_template};
                param_value = tmpl.render(variable_data);
            }
            else {
                param_value = convert_literal_to_protobuf_comatible_literal(given_argument);
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

/// This function returns the "path" as used in the proto files
string get_path_of_var(const SharedVariable &var)
{
    string p = "/";

    for(const string &ns_part : var.var_namespace)
    {
        if (ns_part == "inexor") continue;
        p += ns_part + "/";
    }

    p += var.name;

    return p;
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
    curvariable.set("path", get_path_of_var(var));
    curvariable.set("index", to_string(index));

    add_attached_attributes_templatedata(curvariable, var.attached_attributes, attribute_definitions);

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

mustache::data print_attribute_definitions(const unordered_map<string, attribute_definition> &attribute_definitions)
{
    mustache::data all_attributes_data{mustache::data::type::list};

    int index = 50005;
    // TODO: make this index configurable
    // e.g. by defining an entry in the xml like "<index startval=xy name=optionindex />
    for(auto &deftupel : attribute_definitions)
    {
        auto &def = deftupel.second;
        mustache::data attribute_data{mustache::data::type::object};
        attribute_data.set("name", def.name);
        mustache::data constructor_args_data{mustache::data::type::list};
        if (!def.constructors.empty())
            for (auto &constructor_param : def.constructors.front().constructor_args)
            {
                // only list the constructor params of the first constructor to avoid name clashes between overloaded
                // constructors.
                mustache::data arg_data{mustache::data::type::object};
                arg_data.set("arg_name", constructor_param.name);
                arg_data.set("arg_id", to_string(index++));
                constructor_args_data.push_back(arg_data);
            }
        attribute_data.set("constructor_args", constructor_args_data);
        all_attributes_data.push_back(attribute_data);
    }
    return all_attributes_data;
}

mustache::data print_data(const vector<SharedVariable> &var_occurences,
                          const unordered_map<string, shared_class_definition> &type_definitions,
                          const unordered_map<string, attribute_definition> &attribute_definitions)
{
    mustache::data data{mustache::data::type::object};
    data.set("attribute_definitions", print_attribute_definitions(attribute_definitions));
    data.set("type_definitions", print_type_definitions(type_definitions, attribute_definitions));
    data.set("variables", print_shared_var_occurences(var_occurences, type_definitions, attribute_definitions));

    data.set("file_comment", "// This file gets generated!\n"
            "// Do not modify it directly but its corresponding template file instead!");
    return data;

}
// TODO: classes defined in classes won't have the right type.

} } // ns inexor::gluegen
