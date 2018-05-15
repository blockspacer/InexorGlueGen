#pragma once

#include <kainjow/mustache.hpp>

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <vector>
#include <string>

namespace inexor { namespace gluegen {

class SharedVariable
{

    /// In case this is a NODE_CLASS_SINGLETON and its a template, we link to the shared classes definition of the template arg.
    /// @note This is for handling the edge case of a SharedLists, since there we're interested in the templated arg
    //        (SharedList<player> players).
    //const std::string template_type;

  public:

    /// The literal type (either e.g. "int"/"float".. or the refid of the class)
    const std::string type;

    /// The literal variable name without namespace (e.g. "mapmodel_amount").
    const std::string name;

    /// The namespace the variable got defined in.
    /// @note Mind that the type might be defined in a different namespace (i.e. xy::var_t z::variable)
    const std::vector<std::string> var_namespace;

    /// All attributes attached when instancing this variable (without resolving them.).
    /// e.g. "NoSync()|Persistent()"
    const std::string attached_attributes_literal;

    /// Creates a Node, the type is determined by the cpp literal string.
    /// @param cpp_type The literal type declaration (e.g. "inexor::SharedVar<int>" or the refid)
    /// @param cpp_name The literal variable name (e.g. "mapmodel_amount").
    /// @param var_namespace The namespace of the variable (e.g. "inexor::rendering").
    /// @param attached_attributes_literal The literal of all used attributes (without resolving them.) (e.g. "NoSync()|Persistent()").
 //   SharedVariable(const std::string &cpp_type, const std::string &cpp_name, const std::string &var_namespace,
 //              const std::string &attached_attributes_literal);

    /// Copy constructor, creates a new subtree similar of all childs of the given node.
    /// So this allocates all children again on the heap.
  //  SharedVariable(const SharedVariable &old);


    /// Constructs a new SharedVar after parsing a xml variable node.
    SharedVariable(const pugi::xml_node &var_xml, const std::string &var_namespace);
};

/// Find all marked global variables inside a bunch of AST xml files (as spit out by doxygen) and save them in a vector.
extern const std::vector<SharedVariable> find_shared_var_occurences(const std::vector<std::unique_ptr<pugi::xml_document>> AST_code_xmls);

/// Return the type literal, given a bunch of SharedVariables (possibly with duplicate types).
extern const std::vector<std::string> get_shared_var_types(const std::vector<SharedVariable> all_sharedvars);

/// Returns true if this node is marked to be shared.
extern bool is_marked_variable(const pugi::xml_node &member_xml);

extern kainjow::mustache::data print_shared_var_occurences(const std::vector<SharedVariable> &shared_var_occurences);
        //shared_attribute_definitions);
// Problem: an der stelle nicht attribute definitions inkludieren wollen, aber müssen um jeweils alle attachten attributes zu printen
} } // namespace inexor::gluegen
