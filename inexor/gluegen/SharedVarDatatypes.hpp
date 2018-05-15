#pragma once

#include "inexor/gluegen/SharedVariables.hpp"

#include <kainjow/mustache.hpp>

#include <vector>
#include <string>

namespace inexor { namespace gluegen {


struct shared_class_definition
{
    /// The name of the SharedClass e.g. "Screen".
    std::string class_name;

    /// The reference identification number used by doxygen.
    /// We can search for it and every instance contains this refid somewhere.
    std::string refid;

    /// The namespace of the SharedClass definition e.g. "inexor::metainfo"
    /// @warning this could be some other namespace as the instances one!
    ///          for example you could have a inexor::metainfo::Screen inexor::rendering::screen1;
    std::string definition_namespace;

    /// We REQUIRE the file to be defined in a cleanly includeable headerfile.
    /// (There is no chance of using forward declarations of the class for the synchronisation code.)
    std::string definition_header;

    /// All direct children of this node.
    std::vector<SharedVariable> elements;
};

/// Return a number of parsed shared class definitions, given the literals of relevant types we want to have obtained.
/// @param AST_class_xmls the AST as parsed by doxygen in XML files. Each file corresponds to the definition of a class.
extern std::vector<shared_class_definition>
        find_class_definitions(const std::vector<std::unique_ptr<pugi::xml_document>> &AST_class_xmls,
                                const std::vector<std::string> &shared_var_type_literals);

extern kainjow::mustache::data print_shared_var_type_definitions(std::vector<shared_class_definition> &shared_var_type_definitions);
                                                          //, shared_attribute_definitions);
} } // namespace inexor::gluegen
