#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/parse_helpers.hpp"

#include <kainjow/mustache.hpp>

#include <boost/algorithm/string.hpp>

#include <vector>
#include <unordered_map>
#include <string>
#include <set>

using namespace pugi;
using namespace std;
using namespace boost::algorithm;

namespace inexor { namespace gluegen {

/// Split a variable or class definition name into namespace part and the short name.
/// i.e. split inexor::rendering::Screen into "inexor::rendering" and "Screen"
/// @return std::pair {namespace, name}
std::pair <const vector<string>, const string> split_into_namspace_and_name(const string &full_name)
{
    vector<string> ns_and_name(split_by_delimiter(full_name, "::"));

    const string name{ns_and_name.back()};
    ns_and_name.pop_back();
    return {ns_and_name, name};
};

/// Return the header file a given class was defined in.
/// If the class was not defined in a header, issues an error and quits the program.
const string get_definitions_header_file(const xml_node &compound_xml, const string &classname)
{
    const string definition_header = compound_xml.child("location").attribute("file").value();

    if(contains(definition_header, ".c"))
    {
        std::cerr << "ERROR: SharedClasses can only be defined in cleanly include-able **header**-files\n"
                  << "Class in question is " << classname << std::endl;
        std::exit(1);
    }
    return definition_header;
}

shared_class_definition new_shared_class_definition(const xml_node &compound_xml)
{
    shared_class_definition def;

    def.refid = compound_xml.attribute("id").value();
    string full_name =  get_complete_xml_text(compound_xml.child("compoundname"));
    const auto &name_ns_tuple = split_into_namspace_and_name(full_name);
    def.definition_namespace = name_ns_tuple.first;
    def.class_name = name_ns_tuple.second;
    def.definition_header = get_definitions_header_file(compound_xml, full_name);

    return def;
}


/// If a set of template parameters were given for a class and a set of corresponding types were given for
/// an instance of such a class, the result will be a map, mapping the alias to the real type of the instance.
///
/// This map will be used when constructing any member variables where the type is an alias.
void add_template_type_alias(const xml_node &compound_xml, const SharedVariable::type_node_t *const type,
                             unordered_map<string, const SharedVariable::type_node_t *> &map)
{
    if (!compound_xml.child("templateparamlist"))
        return;
    size_t i = 0;
    for(const auto &template_param : compound_xml.child("templateparamlist").children())
    {
        std::string param_str = template_param.child("type").child_value(); // e.g. "typename U"

        vector<string> param_words(split_by_delimiter(param_str, " ")); // e.g. "typename", "U"
        if (param_words.size() != 2 || (param_words[0] != "class" && param_words[0] != "typename"))
        {
            std::cerr << "ERROR: Template parameters of SharedClasses need to be in form\n"
                      << "'typename PLACEHOLDER' or 'class PLACEHOLDER'\n"
                      << "Class in question is " << compound_xml.child("compoundname").child_value() << std::endl;
            std::exit(1);
        }
        if (type->template_types.size() <= i)
        {
            std::cerr << "ERROR: Template parameters of SharedClass definition does not match instance\n"
                      << "Class in question is " << compound_xml.child("compoundname").child_value() << std::endl;
            std::exit(1);
        }
        map.emplace(param_words[1], &type->template_types[i]);
        i++;
    }
}

void find_class_definitions(const unordered_map<string, unique_ptr<xml_document>> &AST_class_xmls,
                            const std::vector<SharedVariable> &shared_vars,
                            unordered_map<string, shared_class_definition> &class_definitions)
{
    for(const auto &var : shared_vars)
    {
        string var_type_hash = var.type.uniqueID();
        if(class_definitions.count(var_type_hash) != 0)
            // already a known type
            continue;

        if(AST_class_xmls.find(var.type.refid) == AST_class_xmls.end()) {
            std::cerr << "ERROR: variable '" << var.name << "'has been marked for reflection, but type is not known.\n"
                      << "type in question is " << var_type_hash << std::endl;
            continue;
        }
        const xml_node &compound_xml = AST_class_xmls.at(var.type.refid)->child("doxygen").child("compounddef");

        shared_class_definition class_def = new_shared_class_definition(compound_xml);

        // get all template parameters for this class and see what the instance maps them to.
        unordered_map<string, const SharedVariable::type_node_t *>  type_resolve_map;
        add_template_type_alias(compound_xml, &var.type, type_resolve_map);

        // Supported template use cases:
        // 1. class/typename can be used
        // 1b. templated class in templated class has templated member.

        // TODO: Default parameters for templates
        // TODO: non-type template parameters
        // template<int X = 100> can be used (value as default value)
        // template<class X = int> can be used (type as default value)
        // template<class Type1, class Type2 = Type1> can be used (type is other type)
        // empty <> instantion possible when using default values
        /// Spezialisierungen


        // Check all elements of the class definition for markers
        for(const xml_node &var_xml : find_class_member_vars(compound_xml))
        {
            if(not is_marked_variable(var_xml))
                continue;
            SharedVariable element(var_xml, class_def.definition_namespace);
            // if type was not fully resolved, because there was a template alias used,
            // we resolve it.
            if(type_resolve_map.count(element.type.refid) != 0)
            {
                // the type element always ones the type ptr, therefore create a new one.
                element.type = *type_resolve_map[element.type.refid];
            }

            class_def.elements.push_back(std::move(element));
        }
        find_class_definitions(AST_class_xmls, class_def.elements, class_definitions);
        class_definitions.insert({var_type_hash, std::move(class_def)});
    }
}

} } // namespace inexor::gluegen
