#pragma once

#include <string>
#include <vector>

namespace inexor {
namespace gluegen {

struct name_defaultvalue_tupel
{
    std::string name;
    std::string default_value;
};
typedef std::vector<name_defaultvalue_tupel> name_defaultvalue_vector;

/// A declaration of a class with a "SharedVarAttachement" parent class.
///
/// To make our glue code generation more flexible we invented Shared Var Attachement classes,
/// which contain info about how to render stuff and which are used in the C++ Tree API.
/// i.e. Range(), Persistent(), CustomPath()..
///
/// class xy : SharedVarAttachement {
///     xy(T <paramname> = <paramdefvalue>, S <param2name> = <param2defvalue>, ......) {}
///     const char *<membername> = <membertemplate>;
/// };
struct attribute_definition
{
    /// The name of the class/shared attribute
    std::string name;

    /// Whether or not the constructor has default values is important for subsequent
    bool constructor_has_default_values = false;

    /// All constructor arguments: name first, defaultvalue second.
    /// since its an ordered map we have the positions of the arguments.
    // we dont have type deduction!
    name_defaultvalue_vector constructor_args;

    /// "const char *" members are template "partials" (see mustache docs) for our shared declarations.
    ///
    /// They may contain template data entries previously available or those named the same as the constructor parameters.
    name_defaultvalue_vector const_char_members;

    attribute_definition() {}
    attribute_definition(std::string &&class_name) : name(class_name) {}
};
/// Parses all shared attribute ASTs and return the resulting shared_attributes map.
/// Key of the resulting map: name of attribute
extern const std::unordered_map<std::string, attribute_definition>
    parse_shared_attribute_definitions(std::vector<std::unique_ptr<pugi::xml_document>> AST_xmls);


/// A Sharedattribute instance **used** when instancing a variable or class.
/// I.e. "SharedVar<int> xy(Default(0)|NoSync()|Persistent(true))".
/// -> attribute_name = "Persistent" constructor_args.push_back("true").
struct attached_attribute
{
    /// The sharedattributes name.
    std::string name;
    /// The constructor args for the sharedattribute instance.
    std::vector<std::string> constructor_args;
};

/// Parses " NoSync()|Persistent()|Function([] { echo("hello"); })   "
extern std::vector<attached_attribute> parse_attached_attributes_string(std::string attributes_list_str,
                                                                        bool verbose = false);

}
}
