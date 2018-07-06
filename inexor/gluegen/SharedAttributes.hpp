#pragma once

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace inexor {
namespace gluegen {


struct function_parameter
{
    std::string type;
    std::string name;
    std::string default_value;
};

/// A declaration of a class with a "SharedVarAttribute" parent class.
///
/// To make our glue code generation more flexible we invented SharedVarAttribute.
/// Instances of those SharedVarAttribute classes can be used when creating a SharedVar.
/// A SharedVarAttribute contains template data, which can extend the information of the variable it gets attached to.
/// The initializer of any member with "static constexpr char *" type is a template itself and will be rendered into a string.
/// The string will be available inside the templatedata of the variable using
/// 1. the name of the entry of that
/// Fs
/// examples Range(0, 2), Persistent(), CustomPath()..
///
/// class xy : SharedVarAttachement {
///     xy(T <paramname> = <paramdefvalue>, S <param2name> = <param2defvalue>, ......) {}
///     const char *<membername> = <membertemplate>;
/// };
struct attribute_definition
{
    /// The name of the class/shared attribute
    std::string name;

    struct constructor {
        /// All constructor arguments: name first, defaultvalue second.
        /// since its an ordered map we have the positions of the arguments.
        std::vector<function_parameter> constructor_args;

        /// Whether this constructor has arguments with default values or not.
        bool has_default_values = false;
    };
    std::vector<constructor> constructors;

    attribute_definition() {}
    attribute_definition(std::string &&class_name) : name(class_name) {}
};
/// Parses all shared attribute ASTs and return the resulting shared_attributes map.
/// Key of the resulting map: name of attribute
extern const std::unordered_map<std::string, attribute_definition>
    parse_shared_attribute_definitions(const std::vector<std::unique_ptr<pugi::xml_document>> &AST_xmls);

}
}
