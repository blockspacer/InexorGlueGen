
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/parse_helpers.hpp"

#include <kainjow/mustache.hpp>

#include <boost/algorithm/string.hpp>

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <string>


using std::string;
using std::to_string;
using std::vector;
using namespace pugi;
using namespace kainjow;
using namespace boost::algorithm;

namespace inexor { namespace gluegen {

/// Returns true if this node is marked to be shared.
bool is_marked_variable(const xml_node &member_xml)
{
    if(contains(get_complete_xml_text(member_xml.child("initializer")), "reflection_marking"))
        return true;
    return false;
}

/// Returns the vector of the namespace of this AST xml, split by ::
/// There are different AST xmls for not namespaced code and code inside namespaces.
const vector<string> get_namespace_of_namespace_file(const xml_node compound_xml)
{
    if(string(compound_xml.attribute("kind").value()) != "namespace") // not a ns file
        return vector<string>();

    const string ns_str = get_complete_xml_text(compound_xml.child("compoundname"));
    return split_by_delimiter(ns_str, "::");
}

/// The recursively executable part of the type_parser.
/// An item is an entry in a vector of either refids or delimitiers of a type with templates.
/// It takes an iterator and increases it until the complete type_t<....> is handled
/// Afterwards it returns the type.
/// The parent stack is used, since
/// 1. type_t<type2_t<...>, type3_t> shouldn't assign type_3_t as child of type_2_t.
/// 2. type_t<type2_t<type3_t<int>>, type4_t> needs to handle ">>," in one run, since it is one item.
void type_part_parser(vector<xml_node>::const_iterator &it,
                 const vector<xml_node>::const_iterator &end,
                 SharedVariable::type_node_t *parent)
{
    if (!parent) return;

    if (!parent->refid.empty()) {
        parent->template_types.emplace_back();
        parent->template_types.back().parent = parent;
    }
    SharedVariable::type_node_t &node = !parent->refid.empty() ? parent->template_types.back() : *parent;

    node.refid = it->attribute("refid").value();
    // handle case that there was no refid set -> take the value
    if (node.refid.empty())
        node.refid = it->value();

    it++;
    if (it == end) return;

    // the template arguments
    string delimiter = it->value();
    trim(delimiter);
    if (delimiter == "<") {
        type_part_parser(++it, end, &node);
    } else if (delimiter == ",") {
        type_part_parser(++it, end, node.parent);
    } else if (delimiter.find(">") != string::npos) {
        SharedVariable::type_node_t *parent = node.parent;
        for(auto &c : delimiter) {
            if (c == '>') {
                if (!parent || !parent->parent) break;
                parent = parent->parent;
            }
            else if (c == ',') {
                type_part_parser(++it, end, parent);
                break;
            }
        }
        ++it;
    }
}

/// A vector of <refid|'delimiter_sequence'> gets unfolded into a tree of type refid
/// with its template types as children.
SharedVariable::type_node_t type_parser(const vector<xml_node> &type_nodes)
{
    // input e.g. class_out_t< class_a_t, class_b_t<class_b_c_t>>
    // resolved in AST:
    // <ref refid="structclass__out__t" kindref="compound">class_out_t</ref>
    // <
    //    <ref refid="structclass__a__t" kindref="compound">class_a_t</ref>
    //    ,
    //    <ref refid="structclass__b__t" kindref="compound">class_b_t</ref>
    //    <
    //      <ref refid="structclass__b_c__t" kindref="compound">class_b_c_t</ref>
    //    >
    //    ,
    //    <ref refid="structclass__d__t" kindref="compound">class_d_t</ref>
    // >
    //
    // becomes:
    // root.id = class_out_t
    //<
    //   a.id = class_a_t
    //   root.template_types.push_back(a)
    //,
    //   b.id = class_b_t
    //   root.template_types.push_back(b)
    //<
    //      b_c.id = class_b_c_t
    //      b.template_types.push_back(b_c)
    //>,
    //   d.id = class_d_t
    //   root.template_types.push_back(d)
    //>
    SharedVariable::type_node_t root;

    vector<xml_node>::const_iterator it = type_nodes.begin();
    type_part_parser(it, type_nodes.end(), &root);
    return std::move(root);
}

/// Parses " NoSync()|Persistent()|Function([] { echo("hello"); })   "
vector<SharedVariable::attached_attribute> parse_attached_attributes_string(string attributes_list_str, bool verbose = false)
{
    vector<SharedVariable::attached_attribute> attributes;
    const vector<string> attribute_strings_vec(split_by_delimiter(attributes_list_str, "|")); // tokenize

    for(string raw_str : attribute_strings_vec) // e.g. " NoSync() \n" or Range(0, 3) or Persistent(true)
    {
        trim(raw_str);                       // remove any whitespace around normal chars " NoSync(   ) \n" -> "NoSync(   )"
        SharedVariable::attached_attribute attribute;

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

/// Takes xml variable nodes and returns a new SharedVar according to it.
SharedVariable::SharedVariable(const xml_node &var_xml, const vector<string> &var_namespace) :
        name(get_complete_xml_text(var_xml.child("name"))), var_namespace(var_namespace)
{
    // e.g. SharedVar<int>
    const string type_lit = get_complete_xml_text(var_xml.child("type"));
    auto type_childs = var_xml.child("type").children();
    type = type_parser(vector<xml_node>(type_childs.begin(), type_childs.end()));

    // The attached attributes are passed to the reflection_marking.. function as function parameters.
    string initializer = get_complete_xml_text(var_xml.child("initializer"));
    string dummy;
    string attached_attributes_literal = parse_bracket(initializer, dummy, dummy);
    attached_attributes = parse_attached_attributes_string(attached_attributes_literal);
}

/// Find all marked shared vars inside a given document AST.
void find_shared_var_occurences(const pugi::xml_node &compound_xml, std::vector<SharedVariable> &output_list)
{
    // doxygens AST for cpp files is roughly (in xml format):
    // doxygen
    //   compound
    //     section("func")
    //       member
    //       member..
    //     section("var")
    //     section("define")..

    // all vars in one xml file are in the same ns, thanks doxygen.
    const vector<string> ns_of_vars = get_namespace_of_namespace_file(compound_xml);

    for(const auto &section : compound_xml.children("sectiondef"))
    {
        if(string(section.attribute("kind").value()) == "var"
           || string(section.attribute("kind").value()) == "func") // sometimes accessing constructors gets mistakenly recognized as function
        {
            for(const auto &member_xml : section.children("memberdef"))
            {
                if(is_marked_variable(member_xml)) {
                    output_list.push_back(SharedVariable{member_xml, ns_of_vars});
                }
            }
        }
    }
}

const std::vector<SharedVariable> find_shared_var_occurences(const std::vector<std::unique_ptr<pugi::xml_document>> &AST_code_xmls)
{
    std::vector<SharedVariable> buf;
    for(const std::unique_ptr<pugi::xml_document> &fileast : AST_code_xmls)
    {
        const xml_node compound_xml = fileast->child("doxygen").child("compounddef");
        find_shared_var_occurences(compound_xml, buf);
    }
    return buf;
}


} } // namespace inexor::gluegen
