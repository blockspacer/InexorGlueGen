#pragma once

#include <kainjow/mustache.hpp>

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <vector>
#include <string>
#include <cstring>
#include <set>

namespace inexor { namespace gluegen {


struct SharedVariable
{
    /// The type of a variable can include template arguments
    /// i.e. SharedMap<int, SharedVar<int>>
    /// becomes
    ///         SharedMap (refid = class_sharedmap)
    ///         /                           \
    ///     int (refid = classint)     SharedVar (refid = class_sharedvar)
    ///                                      \
    ///                                 int (refid = classint)
    struct type_node_t {
        /// Either the refid of the class or the name of a primitive (int/float/..).
        std::string refid;
        std::vector<type_node_t> template_types;
        type_node_t *parent = nullptr;

        std::string uniqueID() const
        {
            std::string buf = refid;
            for (size_t i = 0; i < template_types.size(); i++)
            {
                if (i == 0) buf += "<";
                buf += template_types[i].uniqueID();
                if (i==template_types.size()-1) buf += ">";
                else buf += ",";
            }
            return buf;
        }
    };

    /// A Sharedattribute instance **used** when instancing a variable or class.
    /// I.e. "SharedVar<int> xy(Persistent(true))".
    /// -> attribute_name = "Persistent" constructor_args.push_back("true").
    struct attached_attribute
    {
        /// The attributes name.
        std::string name;
        /// The constructor args for the attribute instance.
        std::vector<std::string> constructor_args;
    };

    type_node_t type;

    /// The literal variable name without namespace (e.g. "mapmodel_amount").
    const std::string name;

    /// The namespace the variable got defined in.
    /// @note Mind that the type might be defined in a different namespace (i.e. xy::var_t z::variable)
    const std::vector<std::string> var_namespace;

    /// All attributes attached when instancing this variable.
    std::vector<attached_attribute> attached_attributes;

    /// Constructs a new SharedVar after parsing a xml variable node.
    SharedVariable(const pugi::xml_node &var_xml, const std::vector<std::string> &var_namespace);

};

/// Find all marked global variables inside a bunch of AST xml files (as spit out by doxygen) and save them in a vector.
extern const std::vector<SharedVariable> find_shared_var_occurences(const std::vector<std::unique_ptr<pugi::xml_document>> &AST_code_xmls);

/// Returns true if this node is marked to be shared.
extern bool is_marked_variable(const pugi::xml_node &member_xml);

} } // namespace inexor::gluegen
