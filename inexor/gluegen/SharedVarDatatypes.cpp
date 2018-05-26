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

shared_class_definition new_shared_class_definition(const xml_node &compound_xml)
{
    shared_class_definition def;

    def.refid = compound_xml.attribute("id").value();
    string full_name =  get_complete_xml_text(compound_xml.child("compoundname")); // includes the namespace e.g. inexor::rendering::screen

    vector<string> ns_and_name(split_by_delimiter(full_name, "::"));

    def.class_name = ns_and_name.back();
    ns_and_name.pop_back();
    def.definition_namespace = join(ns_and_name, "::");
    def.definition_header = compound_xml.child("location").attribute("file").value();

    if(contains(def.definition_header, ".c"))
    {
        std::cerr << "ERROR: SharedClasses can only be defined in cleanly include-able **header**-files"
                  << std::endl << "Class in question is " << full_name << std::endl;
        std::exit(1);
    }
    return def;
}

void find_class_definitions(const unordered_map<string, unique_ptr<xml_document>> &AST_class_xmls,
                            const std::vector<SharedVariable> &shared_vars,
                            unordered_map<string, shared_class_definition> &class_definitions)
{
    for(const auto &var : shared_vars)
    {
        string var_type_hash = var.type->print();
        if(class_definitions.count(var_type_hash) != 0)
            // already a known type
            continue;

        if(AST_class_xmls.find(var.type->refid) == AST_class_xmls.end()) {
            std::cerr << "ERROR: variable '" << var.name << "'has been marked for reflection, but type is not known.\n"
                      << "type in question is " << var_type_hash << std::endl;
            continue;
        }
        const xml_node &compound_xml = AST_class_xmls.at(var.type->refid)->child("doxygen").child("compounddef");

        shared_class_definition class_def = new_shared_class_definition(compound_xml);

        // Check all elements of the class definition for markers
        for(const xml_node &var_xml : find_class_member_vars(compound_xml))
        {
            if(not is_marked_variable(var_xml))
                continue;
            class_def.elements.push_back(SharedVariable{var_xml, class_def.definition_namespace});
        }
        find_class_definitions(AST_class_xmls, class_def.elements, class_definitions);
        class_definitions.insert({var_type_hash, std::move(class_def)});
    }
}

using namespace kainjow;
/// Create a shared class definition which the
/// \param def
/// \param ctx
/// \param add_instances
/// \return
mustache::data get_shared_class_templatedata(const shared_class_definition &def)
{
    mustache::data cur_definition{mustache::data::type::object};
    // The class needs to be defined in a cleanly includeable header file.
    cur_definition.set("definition_header", def.definition_header);

    cur_definition.set("class_name", def.class_name);
   // cur_definition.set("definition_name_unique", def.get_name_unique());
/*
    add_options_templatedata(cur_definition, def.attached_options, ctx);

    mustache::data all_instances{mustache::data::type::list};

    if(add_instances) for(ShTreeNode *inst_node : def.instances)
        {
            mustache::data cur_instance{mustache::data::type::object};
            add_namespace_seps_templatedata(cur_instance, inst_node->get_namespace());

            // The first parents name, e.g. of inexor::game::player1.weapons.ammo its player1.
            cur_instance.set("name_parent_cpp_short", inst_node->get_name_cpp_short());
            cur_instance.set("name_parent_cpp_full", inst_node->get_name_cpp_full());
            cur_instance.set("instance_name_unique", inst_node->get_name_unique());
            cur_instance.set("path", inst_node->get_path());
            cur_instance.set("index", get_index_incrementer());

            // were doing this for sharedlists, where the first template is relevant.
            if(inst_node->template_type_definition)
            {
                mustache::data dummy_list(mustache::data::type::list);
                dummy_list << get_shared_class_templatedata(inst_node->template_type_definition, ctx, false);
                cur_instance.set("first_template_type", move(dummy_list));
            }

            add_options_templatedata(cur_instance, inst_node->attached_options, ctx);

            all_instances << cur_instance;
        }
    cur_definition.set("instances", all_instances);

    mustache::data members{mustache::data::type::list};

    int local_index = 2;
    for(SharedVariable &child : def.elements)
    {
    //    members << get_shared_var_templatedata(*child, local_index++, ctx);
    }
    cur_definition.set("members", members);
            */
    return cur_definition;
}

mustache::data print_type_definitions(const unordered_map<string, shared_class_definition> &type_definitions)
                                                //, shared_attribute_definitions)
{
    mustache::data sharedclasses{mustache::data::type::list};

    for(const auto &class_def : type_definitions)
    {
        sharedclasses << get_shared_class_templatedata(class_def.second);
    }
    return sharedclasses;

}
} } // namespace inexor::gluegen
