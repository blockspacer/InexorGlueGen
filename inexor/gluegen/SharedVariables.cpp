
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/parse_helpers.hpp"

#include <kainjow/mustache.hpp>

#include <pugiconfig.hpp>
#include <pugixml.hpp>


using namespace pugi;
using namespace kainjow;

namespace inexor { namespace gluegen {

/// Returns true if this node is marked to be shared.
bool is_marked_variable(const xml_node &member_xml)
{
    return contains(get_complete_xml_text(member_xml.child("type")), "SharedVar<");
}

string get_namespace_of_namespace_file(const xml_node compound_xml)
{
    bool isnamespacefile = string(compound_xml.attribute("kind").value()) == "namespace";
    return isnamespacefile ? get_complete_xml_text(compound_xml.child("compoundname")) : "";
}

/// Takes xml variable nodes and returns a new SharedVar according to it.
SharedVariable::SharedVariable(const xml_node &var_xml, const string &var_namespace) :
        type(get_complete_xml_text(var_xml.child("type"))),
        name(get_complete_xml_text(var_xml.child("name")))
{
    string initializer = get_complete_xml_text(var_xml.child("initializer"));
    string dummy;
    attached_attributes_literal = parse_bracket(initializer, dummy, dummy);
}

/// Find all marked shared vars inside a given document AST.
const std::vector<SharedVariable> find_shared_var_occurences(const std::unique_ptr<pugi::xml_document> AST_xml)
{
    // doxygens AST for cpp files is roughly (in xml format):
    // doxygen
    //   compound
    //     section("func")
    //       member
    //       member..
    //     section("var")
    //     section("define")..
    std::vector<SharedVariable> buf;

    const xml_node compound_xml = AST_xml->child("doxygen").child("compounddef");

    // all vars in one xml file are in the same ns, thanks doxygen.
    const string ns_of_vars = get_namespace_of_namespace_file(compound_xml);

    for(const auto &section : compound_xml.children("sectiondef"))
    {
        if(string(section.attribute("kind").value()) == "var"
           || string(section.attribute("kind").value()) == "func") // sometimes accessing constructors gets mistakenly recognized as function
        {
            for(const auto &member : section.children("memberdef"))
            {
                if(is_marked_variable()) {
                    buf.push_back(SharedVariable{member_xml, ns_of_vars});
                }
            }
        }
    }
    return buf;
}

const std::vector<SharedVariable> find_shared_var_occurences(const std::vector<std::unique_ptr<pugi::xml_document>> AST_code_xmls)
{
    std::vector<SharedVariable> buf;
    for(const auto &fileast : AST_code_xmls)
        buf += find_shared_var_occurences(fileast);
    return buf;
}

const std::vector<std::string> get_shared_var_types(const std::vector<SharedVariable> all_sharedvars)
{
    std::vector<std::string> buf;
    for(const auto &var : all_sharedvars)
        buf.push_back(var.type);
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

/// @param local_index if this is different from -1 we set "local_index" with it.
///        otherwise we use the tree_event index counter.. @see add_event_index_templatedata
mustache::data get_shared_var_templatedata(const ShTreeNode &node, int local_index, ParserContext &data)
{
    mustache::data curvariable{mustache::data::type::object};
    // These is a hacky way to distinct between these in pure-data form.. TODO embedd logic in template?
    curvariable.set("is_int", node.type == node.type ? mustache::data::type::bool_true : mustache::data::type::bool_false);
    curvariable.set("is_float", node.get_type_numeric()== node.t_float ? mustache::data::type::bool_true : mustache::data::type::bool_false);
    curvariable.set("is_string", node.get_type_numeric()== node.t_cstring ? mustache::data::type::bool_true : mustache::data::type::bool_false);
    curvariable.set("is_class", node.node_type==ShTreeNode::NODE_CLASS_VAR ? mustache::data::type::bool_false : mustache::data::type::bool_true);


    if(local_index>0) curvariable.set("local_index", std::to_string(local_index));
    curvariable.set("index", get_index_incrementer());
    curvariable.set("path", node.get_path());
    curvariable.set("name_unique", node.get_name_unique());
    curvariable.set("name_cpp_full", node.get_name_cpp_full());
    curvariable.set("name_cpp_short", node.get_name_cpp_short());

    add_namespace_seps_templatedata(curvariable, node.get_namespace());

    add_options_templatedata(curvariable, node.attached_options, data);
    return curvariable;
}

mustache::data fill_templatedata(ParserContext &data)
{
    mustache::data tmpldata{mustache::data::type::object};

    tmpldata.set("file_comment", "// This file gets generated!\n"
                 "// Do not modify it directly but its corresponding template file instead!");

    tmpldata.set("index_reset", get_index_incrementer(true)); // This design is so fucked up.. mustache forces us to though.

    mustache::data sharedvars{mustache::data::type::list};

    for(ShTreeNode *node : data.instances)
    {
        sharedvars << get_shared_var_templatedata(*node, -1, data);
    }
    tmpldata.set("shared_vars", sharedvars);

    mustache::data sharedclasses{mustache::data::type::list};

    for(auto class_def_it : data.shared_class_definitions)
    {
        sharedclasses << get_shared_class_templatedata(class_def_it.second, data);
    }
    tmpldata.set("shared_class_definitions", sharedclasses);

    return tmpldata;
}
        
kainjow::mustache::data print_shared_var_occurences(const std::vector<SharedVariable> &shared_var_occurences,
                                                            shared_attribute_definitions)
{
    mustache::data sharedvars{mustache::data::type::list};

    for(const auto &shared_var : shared_var_occurences)
    {
        sharedvars << get_shared_var_templatedata(*node, -1, data);
    }
    return sharedvars;
}
} } // namespace inexor::gluegen
