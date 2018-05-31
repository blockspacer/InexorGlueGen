#pragma once

#include "inexor/gluegen/SharedVariables.hpp"

#include <kainjow/mustache.hpp>

#include <vector>
#include <unordered_map>
#include <string>

namespace inexor { namespace gluegen {


struct shared_class_definition
{
    /// The name of the SharedClass e.g. "Screen".
    std::string class_name;

    /// The reference identification number used by doxygen.
    /// We can search for it and every instance contains this refid somewhere.
    std::string refid;

    /// The namespace of the SharedClass definition e.g. "inexor, metainfo"
    /// @warning this could be some other namespace as the instances one!
    ///          for example you could have a inexor::metainfo::Screen inexor::rendering::screen1;
    std::vector<std::string> definition_namespace;

    /// We REQUIRE the file to be defined in a cleanly includeable headerfile.
    /// (There is no chance of using forward declarations of the class for the synchronisation code.)
    std::string definition_header;

    /// All direct children of this node.
    std::vector<SharedVariable> elements;
};

/// Return a number of parsed shared class definitions, given a list of sharedvars, which types we want to have obtained.
/// Note: if a classes member is marked, its type will also be appended to the return vector.
/// @param AST_class_xmls the AST as parsed by doxygen in XML files. Each file corresponds to the definition of a class.
///                       the key is the ID of the class (i.e. the type ID).
/// @param shared_vars for each of those, the t of type IDs (as found in the AST) which are relevant.
/// @param class_definitions the map to be filled, key is always the printed out type.
extern void find_class_definitions(const std::unordered_map<std::string, std::unique_ptr<pugi::xml_document>> &AST_class_xmls,
                                   const std::vector<SharedVariable> &shared_vars,
                                   std::unordered_map<std::string, shared_class_definition> &class_definitions);

} } // namespace inexor::gluegen
