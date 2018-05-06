
#include "inexor/gluegen/SharedVariables.hpp"

#include <pugiconfig.hpp>
#include <pugixml.hpp>



namespace inexor { namespace gluegen {


const std::vector<SharedVariable> find_shared_var_occurences(const std::unique_ptr<pugi::xml_document> AST_xml)
{

}

const std::vector<SharedVariable> find_shared_var_occurences(const std::vector<std::unique_ptr<pugi::xml_document>> AST_code_xmls)
{
    std::vector<SharedVariable> buf;
    for(auto &fileast : AST_code_xmls)
        buf += find_shared_var_occurences(fileast);
    return buf;
}

const std::vector<std::string> get_shared_var_types(const std::vector<SharedVariable> all_sharedvars)
{
    std::vector<std::string> buf;
    for(auto &var : all_sharedvars)
    {
        buf
    }
}

        // -------------------------------
/// Adds the entries namespace_sep_{open|close} to the templatedata given.
/// open is e.g. "namespace inexor { namespace rendering {" close is "} }"
void add_namespace_seps_templatedata(TemplateData &templdata, string ns_str)
{
    vector<string> ns(split_by_delimiter(ns_str, "::"));

    TemplateData namespace_sep_open{kainjow::mustache::lambda{
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

    TemplateData namespace_sep_close{kainjow::mustache::lambda{
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

/// Index is a very special data entry: every time it gets referenced it gets iterated!
kainjow::mustache::partial get_index_incrementer(bool reset = false)
{
    return kainjow::mustache::partial([reset]() {
        static int count = 21;
        if(reset) count = 20;
        return to_string(count++);
    });
}

/// @param local_index if this is different from -1 we set "local_index" with it.
///        otherwise we use the tree_event index counter.. @see add_event_index_templatedata
TemplateData get_shared_var_templatedata(const ShTreeNode &node, int local_index, ParserContext &data)
{
    TemplateData curvariable{TemplateData::type::object};
    // These is a hacky way to distinct between these in pure-data form.. TODO embedd logic in template?
    curvariable.set("is_int", node.type == node.type ? TemplateData::type::bool_true : TemplateData::type::bool_false);
    curvariable.set("is_float", node.get_type_numeric()== node.t_float ? TemplateData::type::bool_true : TemplateData::type::bool_false);
    curvariable.set("is_string", node.get_type_numeric()== node.t_cstring ? TemplateData::type::bool_true : TemplateData::type::bool_false);
    curvariable.set("is_class", node.node_type==ShTreeNode::NODE_CLASS_VAR ? TemplateData::type::bool_false : TemplateData::type::bool_true);


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

TemplateData fill_templatedata(ParserContext &data)
{
    TemplateData tmpldata{TemplateData::type::object};

    tmpldata.set("file_comment", "// This file gets generated!\n"
                 "// Do not modify it directly but its corresponding template file instead!");

    tmpldata.set("index_reset", get_index_incrementer(true)); // This design is so fucked up.. mustache forces us to though.

    TemplateData sharedvars{TemplateData::type::list};

    for(ShTreeNode *node : data.instances)
    {
        sharedvars << get_shared_var_templatedata(*node, -1, data);
    }
    tmpldata.set("shared_vars", sharedvars);

    TemplateData sharedclasses{TemplateData::type::list};

    for(auto class_def_it : data.shared_class_definitions)
    {
        sharedclasses << get_shared_class_templatedata(class_def_it.second, data);
    }
    tmpldata.set("shared_class_definitions", sharedclasses);

    return tmpldata;
}

} } // namespace inexor::gluegen
