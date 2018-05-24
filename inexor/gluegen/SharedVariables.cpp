
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

string get_namespace_of_namespace_file(const xml_node compound_xml)
{
    bool isnamespacefile = string(compound_xml.attribute("kind").value()) == "namespace";
    return isnamespacefile ? get_complete_xml_text(compound_xml.child("compoundname")) : "";
}

/// The recursively executable part of the nested_type2typetree parser.
/// An item is an entry in a vector of either refids or delimitiers of a type with templates.
/// It takes an iterator and increases it until the complete type_t<....> is handled
/// Afterwards it returns the type.
/// The parent stack is used, since
/// 1. type_t<type2_t<...>, type3_t> shouldn't assign type_3_t as child of type_2_t.
/// 2. type_t<type2_t<type3_t<int>>, type4_t> needs to handle ">>," in one run, since it is one item.
SharedVariable::type_node_t *get_type_tree(vector<xml_node>::const_iterator &it,
                 const vector<xml_node>::const_iterator &end,
                 vector<SharedVariable::type_node_t *> &parent_stack)
{
    // the actual name
    SharedVariable::type_node_t *node = new SharedVariable::type_node_t();
    node->refid = it->attribute("refid").value();
    if (!parent_stack.empty())
        parent_stack.back()->template_types.push_back(node);

    it++;
    if (it == end) {
        return node;
    }
    // the template arguments
    string delimiter = it->value();
    trim(delimiter);
    if (delimiter == "<") {
        parent_stack.push_back(node);
        get_type_tree(++it, end, parent_stack);
    } else if (delimiter == ",") {
        get_type_tree(++it, end, parent_stack);
    } else if (delimiter.find(">") != string::npos) {
        for(auto &c : delimiter) {
            if (c == '>') {
                if (parent_stack.empty()) break;
                parent_stack.pop_back();
            }
            else if (c == ',') {
                get_type_tree(++it, end, parent_stack);
                break;
            }
        }
        ++it;
    }
    return node;
}

/// A vector of <refid|'delimiter_sequence'> gets unfolded into a tree of type refid
/// with its template types as children.
SharedVariable::type_node_t *get_type_tree(const vector<xml_node> &type_nodes)
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
    vector<SharedVariable::type_node_t *> parent_stack;

    vector<xml_node>::const_iterator it = type_nodes.begin();
    return get_type_tree(it, type_nodes.end(), parent_stack);
}

/// Takes xml variable nodes and returns a new SharedVar according to it.
SharedVariable::SharedVariable(const xml_node &var_xml, const string &var_namespace) :
        name(get_complete_xml_text(var_xml.child("name")))
{
    // e.g. SharedVar<int>
    const string type_lit = get_complete_xml_text(var_xml.child("type"));
    auto type_childs = var_xml.child("type").children();
    type = get_type_tree(vector<xml_node>(type_childs.begin(), type_childs.end()));

    // The attached attributes are passed to the reflection_marking.. function as function parameters.
    string initializer = get_complete_xml_text(var_xml.child("initializer"));
    string dummy;
    attached_attributes_literal = parse_bracket(initializer, dummy, dummy);
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
    const string ns_of_vars = get_namespace_of_namespace_file(compound_xml);

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

// -------------------------------
using namespace kainjow;
/// Adds the entries namespace_sep_{open|close} to the templatedata given.
/// open is e.g. "namespace inexor { namespace rendering {" close is "} }"
void add_namespace_seps_templatedata(mustache::data &templdata, string ns_str)
{
    vector<string> ns(split_by_delimiter(ns_str, "::"));

    mustache::data namespace_sep_open{kainjow::mustache::lambda{
        [ns](const string&)
    {
        std::stringstream ss;
        for(auto &tok : ns)
        {
            if(tok.empty())continue;
            ss << "namespace ";
            ss << tok;
            ss << " { ";
        }
        return ss.str();
    }}};

    mustache::data namespace_sep_close{kainjow::mustache::lambda{
        [ns](const std::string&)
    {
        std::stringstream ss;
        for(auto &tok : ns)
        {
            if(tok.empty())continue;
            ss << " }";
        }
        return ss.str();
    }}};

    templdata.set("namespace_sep_open", namespace_sep_open);
    templdata.set("namespace_sep_close", namespace_sep_close);
}
/*
TODO:

partial instead:
"namespace_sep_open": "{{#namespace}}{{name}} { {{/namespace}}"
"namespace_sep_close": "{{#namespace}} } {{/namespace}}"
"full_name": "{{#namespace}}{{.}}::{{/namespace}}{{name}}"
"path": "
"name_unique"

first:
  str name, list namespace, string full_type
  char *: SharedVar<char *> is_sharedvar. member: is_cstring
  int: SharedVar<int> important: inner, ignored: outer
  list: SharedList<vector> important: outer, ignored: inner
  list: SharedList<list>
  map: SharedMap<map>
  map: SharedMap<unordered_map>
 */

/// Index is a very special data entry: every time it gets referenced it gets iterated!
mustache::partial get_index_incrementer(bool reset = false)
{
    return kainjow::mustache::partial([reset]() {
        static int count = 21;
        if(reset) count = 20;
        return to_string(count++);
    });
}

mustache::data get_shared_var_templatedata(const SharedVariable &var)
{
    mustache::data curvariable{mustache::data::type::object};
    // These is a hacky way to distinct between these in pure-data form.. TODO embedd logic in template?
//    curvariable.set("is_int", node.type == node.type ? mustache::data::type::bool_true : mustache::data::type::bool_false);
//    curvariable.set("is_float", node.get_type_numeric()== node.t_float ? mustache::data::type::bool_true : mustache::data::type::bool_false);
//    curvariable.set("is_string", node.get_type_numeric()== node.t_cstring ? mustache::data::type::bool_true : mustache::data::type::bool_false);
//    curvariable.set("is_class", node.node_type==ShTreeNode::NODE_CLASS_VAR ? mustache::data::type::bool_false : mustache::data::type::bool_true);
    //if(local_index>0) curvariable.set("local_index", std::to_string(local_index));
//    curvariable.set("path", node.get_path());
//    curvariable.set("name_unique", node.get_name_unique());
//    curvariable.set("name_cpp_full", node.get_name_cpp_full());
//    curvariable.set("name_cpp_short", node.get_name_cpp_short());
    mustache::data ns{mustache::data::type::list};
   // ns.push_back( var.var_namespace)
    curvariable.set("var_namespace", ns);
    curvariable.set("var_name", var.name);
    curvariable.set("index", get_index_incrementer());

  //  add_namespace_seps_templatedata(curvariable, node.get_namespace());

   // add_options_templatedata(curvariable, node.attached_options, data);
    return curvariable;
}
        
kainjow::mustache::data print_shared_var_occurences(const std::vector<SharedVariable> &shared_var_occurences)
{
    mustache::data sharedvars{mustache::data::type::list};

    for(const auto &shared_var : shared_var_occurences)
    {
        sharedvars << get_shared_var_templatedata(shared_var);
    }
    return sharedvars;
}

} } // namespace inexor::gluegen
