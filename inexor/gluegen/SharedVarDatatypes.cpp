#include "inexor/gluegen/SharedVarDatatypes.hpp"

#include <vector>
#include <string>

namespace inexor { namespace gluegen {


/// Create a shared class definition which the
/// \param def
/// \param ctx
/// \param add_instances
/// \return
TemplateData get_shared_class_templatedata(shared_class_definition *def, ParserContext &ctx, bool add_instances = true)
{
    TemplateData cur_definition{TemplateData::type::object};
    // The class needs to be defined in a cleanly includeable header file.
    cur_definition.set("definition_header_file", def->containing_header);

    // TODO: recognize the innerclass case!
    // The name of the class with leading namespace.
    cur_definition.set("definition_name_cpp", def->get_name_cpp_full());
    cur_definition.set("definition_name_unique", def->get_name_unique());

    add_options_templatedata(cur_definition, def->attached_options, ctx);

    TemplateData all_instances{TemplateData::type::list};

    if(add_instances) for(ShTreeNode *inst_node : def->instances)
        {
            TemplateData cur_instance{TemplateData::type::object};
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
                TemplateData dummy_list(TemplateData::type::list);
                dummy_list << get_shared_class_templatedata(inst_node->template_type_definition, ctx, false);
                cur_instance.set("first_template_type", std::move(dummy_list));
            }

            add_options_templatedata(cur_instance, inst_node->attached_options, ctx);

            all_instances << cur_instance;
        }
    cur_definition.set("instances", all_instances);

    TemplateData members{TemplateData::type::list};

    int local_index = 2;
    for(ShTreeNode *child : def->nodes)
    {
        members << get_shared_var_templatedata(*child, local_index++, ctx);
    }
    cur_definition.set("members", members);
    return cur_definition;
}

} } // namespace inexor::gluegen
