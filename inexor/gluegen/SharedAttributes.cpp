#include "inexor/gluegen/parse_helpers.hpp"
#include "inexor/gluegen/fill_templatedata.hpp"
#include "inexor/gluegen/tree.hpp"
#include "inexor/gluegen/ParserContext.hpp"

#include <boost/algorithm/string.hpp>

#include <vector>
#include <string>
#include <unordered_set>

using namespace std;

namespace inexor { namespace gluegen {

/// This function searches constructors of the sharedattributes class in the AST, looks up the args of the constructor and saves those in opt.
///
/// @param opt the class (derived from a common base class called Sharedattribute) in parsed form.
/// We require to have all constructor arguments named the same.
/// We furthermore require to have default values for either all or no constructor arguments.
/// + all default_values across all constructors need to be the same.
/// Error if those requirements aren't met.
name_defaultvalue_vector find_so_constructors_args(attribute_definition &opt, const xml_node &compound_xml)
{
    name_defaultvalue_vector constructor_args;

    auto constructor_vector = find_class_constructors(compound_xml);
    if(constructor_vector.empty()) return name_defaultvalue_vector();

    // The first constructor fills the argument list, the following just control whether their lists are equal.
    const xml_node first_constructor = constructor_vector.front();

    for(const xml_node param : first_constructor.children("param"))
    {
        name_defaultvalue_tupel arg;
        arg.name = get_complete_xml_text(param.child("declname"));
        arg.default_value = get_complete_xml_text(param.child("defval"));
        std::cout << "arg.name: " << arg.name << std::endl;

        if(!arg.default_value.empty())
        {
            opt.constructor_has_default_values = true;
            std::string temp;
            arg.default_value = parse_bracket(arg.default_value, temp, temp); // fu_cast<float>( "{{index}}\n{{name}}" ) -> "{{index}}\n{{name}}"
            trim(arg.default_value);                                          // remove whitespace around "{{index}}\n{{name}}"
            remove_surrounding_quotes(arg.default_value);                     // -> {{index}}'\''n'{{name}}
            unescape(arg.default_value);                                      // replace "\\n" with newline char.
        }
        constructor_args.push_back(arg);
    }
    return constructor_args;

    // TODO Error checking:
    // * check qualness of constructor argument names of the different constructors
    // * allow only: all constructor arguments or no constructor args have default values!
    //    * (we dont want control individually whether constructor param have default values, since
    //       that may require us to match different constructors when parsing the instances. that would be too complex)
}

name_defaultvalue_vector find_attributes_class_const_char_members(attribute_definition &opt, const xml_node &compound_xml)
{
    name_defaultvalue_vector const_char_members;

    for(auto var_xml : find_class_member_vars(compound_xml))
    {
        if(!contains(get_complete_xml_text(var_xml.child("type")), "char")) continue;

        name_defaultvalue_tupel var;
        var.name = get_complete_xml_text(var_xml.child("name"));
        var.default_value = get_complete_xml_text(var_xml.child("initializer"));
        remove_leading_assign_sign(var.default_value);
        trim(var.default_value); //remove whitespace
        remove_surrounding_quotes(var.default_value);
        const_char_members.push_back(var);
    }

    return const_char_members;
}

/// This function parses an attribute_definition xml node and save it to our attribute_definitions map.
///
/// We require to have all constructor arguments named the same.
/// We furthermore require to have default values for either all or no constructor arguments.
/// + all default_values across all constructors need to be the same.
// TODO we do not handle namespaces for this correctly atm.
// TODO: Error if those requirements aren't met.
const attribute_definition parse_shared_attribute_definition(const xml_node &compound_xml)
{
    attribute_definition opt(get_complete_xml_text(compound_xml.child("compoundname")));
    std::cout << "Sharedattribute-derived class definition found: " << opt.name << std::endl;

    opt.constructor_args = find_so_constructors_args(opt, compound_xml);
    opt.const_char_members = find_attributes_class_const_char_members(opt, compound_xml);

    for(auto member : opt.const_char_members)
        std::cout << "['const char *'-members of " << opt.name << "] " << member.name << " = " << member.default_value << std::endl;
    return opt;
}

std::unordered_map<std::string, attribute_definition>
    parse_shared_attribute_definitions(const std::vector<std::unique_ptr<pugi::xml_document>> AST_xmls)
{
    std::unordered_map<std::string, attribute_definition> attribute_definitions;
    for(const auto &ast : AST_xmls) {
        auto opt = parse_shared_attribute_definition(compound_xml);
        attribute_definitions[opt.name] = opt;
    }
    return attribute_definitions;
}

/// Parses " NoSync()|Persistent()|Function([] { echo("hello"); })   "
vector<attached_attribute> parse_attached_attributes_string(string attributes_list_str, bool verbose)
{
    vector<attached_attribute> attributes;

    const vector<string> attribute_strings_vec(split_by_delimiter(attributes_list_str, "|")); // tokenize

    for(string raw_str : attribute_strings_vec) // e.g. " NoSync() \n" or Range(0, 3) or Persistent(true)
    {
        trim(raw_str);                       // remove any whitespace around normal chars " NoSync(   ) \n" -> "NoSync(   )"
        attached_attribute attribute;

        string temp;
        string argsstr = parse_bracket(raw_str, attribute.name, temp);       // from Range(0, 3) we get "0, 3"
        attribute.constructor_args = tokenize_arg_list(argsstr);             // "0", " 3"
        for(string &arg : attribute.constructor_args)                        // "0", "3"
            trim(arg);


        if(verbose)
        {
            std::cout << "string: " << raw_str << std::endl;
            std::cout << "opt name: " << attribute.name << std::endl << "args:";
            for(auto &i : attribute.constructor_args)
                std::cout << " " << i;
            std::cout << std::endl;
        }

        attributes.push_back(attribute);
    }
    return attributes;
}

void add_rendered_template_hybrids(const attribute_definition &opt, TemplateData &variable, const TemplateData constructor_args_data)
{
    for(const auto &member : opt.const_char_members)
    {
        TemplateData template_hybrid_rendered{TemplateData::type::object};
        MustacheTemplate tmpl{member.default_value};             // the default value of the const char member acts as template.
        template_hybrid_rendered["s"] = tmpl.render(constructor_args_data); // gets rendered using the data from the constructor args
        variable[member.name] << template_hybrid_rendered; // and the rendered string is templatedata for the variable (key = <name> from "const char *<name>"

        //std::cout << "rendered " << template_hybrid.name << " : " << template_hybrid_rendered["s"].stringValue() << std::endl;
    }
}

/// A shared attribute can have default values. All Classes WITHOUT having this attribute attached will use the attributes default values then!
void add_attributes_with_default_values_templatedata(TemplateData &variable, ParserContext &data)
{
    for(auto reg_attribute : data.attribute_definitions)
    {
        attribute_definition &opt = reg_attribute.second;
        if(!opt.constructor_has_default_values) continue;
        for(auto arg : opt.constructor_args)
        {
            MustacheTemplate tmpl{arg.default_value};
            variable[arg.name] = tmpl.render(variable); // add templatedata for this
        }
    }
}

/// Add templatedata for this shared variable coming from shared attributes.
void add_attributes_templatedata(TemplateData &variable, vector<attached_attribute> attached_attributes, ParserContext &data)
{
    // renders: 1. fill data:
    //             - take node constructor args, compare it with attribute_definition names [done]
    //             - create TemplateData instance for every constructor param
    //            1b. default values
    //              - if attribute_definition has default values add default value TemplateData
    //              - 2 cases for default values: values or templates (special: char * get rendered nontheless)
    //          2. render templates attributeclass.templatehybrids().templatestr; templatehybrid { name, templatestr } with passed TemplateData
    //             add rendered templates to passed TemplateData

    // sharedvar-list
    //    <template_hybrid_name>-list
    //       sharedattributeinstance1 // e.g. NoSync()
    //       sharedattributeinstance2 // e.g. Persistent()
    //       sharedattributeinstance3 // e.g. Range(0, 3)


    // add template data from constructor arguments:

    TemplateData constructor_args_data = variable;

    add_attributes_with_default_values_templatedata(constructor_args_data, data);
    for(auto node_attribute : attached_attributes) // a node attribute is one instance of a sharedattribute attached to a node (a SharedVar or SharedClass)
    {
        auto find_in_declarations = data.attribute_definitions.find(node_attribute.name);
        if(find_in_declarations == data.attribute_definitions.end()) continue; // this argument is not corresponding with the name of a sharedattribute.
       // std::cout << "node: " << node.get_name_cpp_full() << " attribute: " << node_attribute.name << std::endl;

        const attribute_definition &opt = find_in_declarations->second;
        // if(opt.constructor_args.size() >= node_attribute.constructor_args.size()) throw exception("");

        for(size_t i = 0; i < node_attribute.constructor_args.size(); i++) // loop instance constructor args, take names from defintion of the constructor args.
        {
            if(i >= opt.constructor_args.size()) break; // more arguments used for the instance than the declaration allows.
            constructor_args_data[opt.constructor_args[i].name] = node_attribute.constructor_args[i];
        }
    }

    // render template_hybrids and add them to the templatedata of the variable:

    // add empty lists to the variable key=the name
    for(auto opt : data.attribute_definitions)
        for(auto member : opt.second.const_char_members)
            variable[member.name] = TemplateData(TemplateData::type::list);

    unordered_set<string> rendered_th_names;
    // render th's occuring in the constructor
    for(auto node_attribute : attached_attributes)
    {
        auto find_in_definitions = data.attribute_definitions.find(node_attribute.name);
        if(find_in_definitions == data.attribute_definitions.end()) continue;
        add_rendered_template_hybrids(find_in_definitions->second, variable, constructor_args_data);
        rendered_th_names.emplace(find_in_definitions->second.name);
    }

    // render th's which have default values and havent been rendered before.
    for(auto reg_attribute : data.attribute_definitions)
    {
        attribute_definition &opt = reg_attribute.second;
        auto find_in_instances = rendered_th_names.find(opt.name);
        if(find_in_instances == rendered_th_names.end() && opt.constructor_has_default_values)
            add_rendered_template_hybrids(opt, variable, constructor_args_data);
    }
}

} } // namespace inexor::gluegen
