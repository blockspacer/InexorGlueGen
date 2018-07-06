
#include "inexor/gluegen/SharedAttributes.hpp"
#include "inexor/gluegen/parse_helpers.hpp"

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <boost/algorithm/string.hpp>

#include <vector>
#include <string>
#include <iostream>

using namespace std;
using namespace pugi;
using namespace boost::algorithm;

namespace inexor { namespace gluegen {

/// This function searches constructors of the sharedattributes class in the AST, looks up the args of the constructor and saves those in opt.
///
/// @param opt the class (derived from a common base class called Sharedattribute) in parsed form.
/// We require to have all constructor arguments named the same.
/// We furthermore require to have default values for either all or no constructor arguments.
/// + all default_values across all constructors need to be the same.
/// Error if those requirements aren't met.
function_parameter parse_constructors_arg(const xml_node &arg_xml)
{
        function_parameter arg;
        arg.name = get_complete_xml_text(arg_xml.child("declname"));
        arg.type = get_complete_xml_text(arg_xml.child("type"));

        string raw_default_value = get_complete_xml_text(arg_xml.child("defval"));
        if(!raw_default_value.empty())
        {
            std::string dummy;

            // remove possible cast operations
            // i.e. fu_cast<float>( "{{index}}\n{{name}}" ) -> ' "{{index}}\n{{name}}" '
            arg.default_value = parse_bracket(raw_default_value, dummy, dummy);

            // remove whitespace around the argument
            trim(arg.default_value);

            // -> {{index}}'\''n'{{name}}
            remove_surrounding_quotes(arg.default_value);

            // replace "\\n" with newline char.
            unescape(arg.default_value);
        }
        std::cout << "Constructor Argument Name: " << arg.name
                   << (arg.default_value.empty() ? "" : " (default: "+arg.default_value+", raw: "+raw_default_value+")")
                   << std::endl;

        return arg;
}

const attribute_definition::constructor parse_constructor(const xml_node &constructor_xml)
{
    attribute_definition::constructor constr;

    std::vector<function_parameter> constructor_args;

    for(const xml_node &param : constructor_xml.children("param")) {
        constr.constructor_args.push_back(parse_constructors_arg(param));
    }

    // last arg must have a default value..
    if (!constr.constructor_args.empty() && !constr.constructor_args.back().default_value.empty())
        constr.has_default_values = true;

    return constr;
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
    std::cout << "Attribute class found: " << opt.name << std::endl;

    for (const auto &constructor_xml : find_class_constructors(compound_xml))
        opt.constructors.push_back(parse_constructor(constructor_xml));

    return opt;
}

const unordered_map<string, attribute_definition>
    parse_shared_attribute_definitions(const vector<unique_ptr<xml_document>> &AST_xmls)
{
    unordered_map<string, attribute_definition> attribute_definitions;
    for(const auto &ast : AST_xmls) {
        xml_node compound_ast = ast->child("doxygen").child("compounddef");
        auto opt = parse_shared_attribute_definition(compound_ast);
        attribute_definitions[opt.name] = opt;
    }
    return attribute_definitions;
}

} } // namespace inexor::gluegen
