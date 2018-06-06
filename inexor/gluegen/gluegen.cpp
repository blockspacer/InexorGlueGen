
#include "inexor/gluegen/print_data.hpp"
#include "inexor/gluegen/render_files.hpp"
#include "inexor/gluegen/SharedAttributes.hpp"
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/gluegen/ASTs.hpp"

#include <boost/program_options.hpp>

#include <kainjow/mustache.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>


using namespace inexor::gluegen;
namespace po = boost::program_options;
using std::string;
using std::unordered_map;
using std::vector;
using namespace kainjow;

void usage(const std::string &ex, const po::options_description &params) {
    std::cerr
        << "Inexor GlueGen       Codegenerator which takes an AST produced by Doxygen, finds marked variables "
                "and generates arbitrary code based on it.\n\n\n"
        << params << "\n";
}


/// Our generation of glue code works in 3 steps:
///   1) use doxygen to parse the source and spit out the AST in a XML
///   2) use find any Shared Declarations in this AST (parsing the XML)
///   3) use the gathered data to render the templates
int main(int argc, const char **argv)
{
    // Parse and handle command line arguments

    po::variables_map cli_config;
    po::options_description params("PARAMETERS");
    params.add_options()
        ("help", "Print this help message")
        ("template_file", po::value<std::vector<std::string>>()->multitoken()->composing()->required(),
             "XML file(s) which contain the sections \"partials\" and \"file\" which contain mustache template code.\n"
             "Each entry in there is named.\n"
             "The name of the entry in \"partials\" becomes the name of the partial.\n"
             "The name of the \"file\" entry becomes the filename of the generated file.")
        ("partial_file", po::value<std::vector<std::string>>()->multitoken()->composing(),
             "XML file(s) which contains a list with named entries.\n"
             "The name of the entry becomes the name of a partial which will be available in each <template_file>.")

        ("doxygen_AST_folder", po::value<string>()->required(), "The folder containing the doxygen xml (AST) output. \n"
              "We scan those XML files for Shared Declarations");

    std::string exec{argv[0]};

    vector<string> args(argv+1, argv+argc);
    std::cout << "Used command line options: \n";
    for(auto arg : args) std::cout << arg << "\n";
    try {
        po::parsed_options parsed = po::command_line_parser(args).options(params).run();
        po::store(parsed, cli_config);

        if(cli_config.count("help"))
        {
            usage(exec, params);
            return 0;
        }

        po::notify(cli_config);
    } 
    catch(po::error &e) {
        std::cerr << "Failed to parse the main args: " << e.what() << "\n\n";
        usage(exec, params);
        return 1;
    }

    const vector<string> template_files = cli_config["template_file"].as<vector<string>>();
    const vector<string> partial_files = cli_config.count("partial_file") ? cli_config["partial_file"].as<vector<string>>() : vector<string>();
    const string xml_AST_folder = cli_config["doxygen_AST_folder"].as<string>();

    ASTs code;
    code.load_from_directory(xml_AST_folder);

    auto shared_attribute_definitions = parse_shared_attribute_definitions(code.attribute_class_xmls);

    auto var_occurences = find_shared_var_occurences(code.code_xmls);

    unordered_map<string, shared_class_definition> type_definitions;
    find_class_definitions(code.class_xmls, var_occurences, type_definitions);

    // add print functions to each type.
    mustache::data template_base_data{mustache::data::type::object};
    template_base_data.set("type_definitions", print_type_definitions(type_definitions));
    template_base_data.set("variables", print_shared_var_occurences(var_occurences, type_definitions));

    template_base_data.set("file_comment", "// This file gets generated!\n"
            "// Do not modify it directly but its corresponding template file instead!");

    render_files(template_base_data, partial_files, template_files);


    // Read the list of variables
// for each global var
// - make the type accessible
//     make accessible ==
//       - generate two messages for each
//          1. one_element_of_<class_name>[_<template_type>]
//          2. every_element_of_<class_name>[_<template_type>]
//       - generate two respective handle_message_<class_name>[_<template_type>] c functions
//           - make it generically take as input a pointer to such a class (and a subindex)
//       - generate a connect_<class_name>[_<template_type>]
//       - save where it can be included
//       - save the namespace of the type
// - add an entry to "handle_global_var"
// - add an entry to "global_var_msg"
// - s
// - take care that each incoming message gets handled
/*
.sub
    ASTs = vector<class_name|file_name, AST)

  --- finder -->

    definitions and global_variables
        .sub
            file_ASTs
            -> find_global_var_occurences
            global_var_occurences = tuple(string refid/type, name, string attached_options)
            vector<global_var_occurences>
        ..sub
        .sub
            class_ASTs
            -> find_relevant_class_definitions
            (relevant_?)class_definition = refid, class_name, elements (= vector<tuple<string name, string type_id/builtin_name)
        ..sub
        .sub
            class_ASTs
           -> find_shared_options
            shared_option_definiton = refid, class_name, elements (= vector<tuple<string name, string type_id/builtin_name)
        ..sub

 // --- resolver --> not necessary
 //   global_var_tree = vector<varname, type, subelement_tree>

  --- data_printer -->
    template_data = tree<string> data
..sub
.sub
    template =  xml (filename + partials (zu welchem template_data element jeweils mitspeichern) + file_template)
    vector<template> file_templates
..sub

--- file_render -->

generated_files

// Problem mit der Darstellung:
- Ich will meine Sachen "besser" ordnen als so. Andere Dateinamen z.b.
- daten von davor und davor davor könnten beide in schritt gebraucht werden.

-> Prozessablauf (Datenflow) wird zu Codestruktur indem man ähnliche Sachen gruppiert.
ALso nicht Finder.cpp(find_sharedvars, find_typedefinitions), sondern typedefinitions.find()

////
Problembeschreibung:
Use cases -> Requirements        == Erkenntnisgewinn ==>
Implementation:
Dataflow ->
partitions                       == Refinement/Refactoring(establishing patterns) ==>

partition data flow, partition specific use case ->
search for similar use cases ->
generalization + putting adapters in place
// TODO:
1. write sharedvar class to allow reflection markers
2. write partials

*/
// Later:
//   - add SharedOptions
//   - take care of saving the real typename when dealing with nested class definitions
//         def x { def y };
//         x::y a;
//         x is not the namespace, but the containing class.

/*
 * a list with special types can be defined, which either define an endpoint or a not-endpoint if it is a template
 *
* end:
 *      Vater ist gekennzeichnet, kind nicht: endpoint
 *      geht danach nicht weiter, weil nächstes kein element markiert hat.
 *      SharedVar<x>... x in int, string, char *, float, bool == class without usable elements
 *      unmarkiert?: int, string, char *, ...
 *      unmarkiert ->
 *      markiert: SharedVar
* bridge:
 *      vater markiert, kind markiert
 *      is_player_t
* wrapper:
 *      markiert: SharedList<SharedVar<int>>
 *      element: std::list<SharedVar<int>> markiert, aber könnte list oder was auch immer sein.
 *
 *
 *      vater wie "sharedlist", wird gekennzeichnet, kind nicht.
 *      geht danach weiter, da kind markiert (als nicht-interessant? oder einfach in template nicht kind immer über namen sondern
 *      auch als anonymous_ones)
 *      sharedlist, sharedmap, sharedvector
* zwischen:
 *      pointer (autoderefernce?)
 *      enum (expr unfolding,
 *
 * synccode anhängen
 *      mittel:
 *      friend class
 *      problem: immer alles in class
 *      positiv: auch list, map, vector machbar?
 *
 *
 *      TODO:
 *      1. class member type lookup with refid
 *      3. <typename == xy> in templatedata
 *      2. template wird aufgelöst
 *      4. children in templatedata
 *
 *      3. rumprobieren mit markierung und wrapper und endpoint szenario (zuerst endpoint)
 *
 *      wieso hab ich umgestellt?
 *      - weil ich was gemacht hab -> ein eintrag weniger wichtig für mich ab jetzt
 *      - weil ich gemerkt hab, dass man noch zwischenschritt braucht
 *      - weil ich dachte, der eine zwischenschritt kommt besser vor dem anderen
 *
 *      abstraktionspfad in todoliste inexor/gluegen/rendering-template-machen ist große schublade, mit kleineren schubladen
 */

/*
TODO:

partial instead:
"namespace_sep_open": "{{#namespace}}{{name}} { {{/namespace}}"
"namespace_sep_close": "{{#namespace}} } {{/namespace}}"
"full_name": "{{#namespace}}{{.}}::{{/namespace}}{{name}}"
"path": "
"name_unique"

first:
str name, list namespace, string full_type
char *: SharedVar<char *> is_sharedvar. member: is_cstring
int: SharedVar<int> important: inner, ignored: outer
list: SharedList<vector> important: outer, ignored: inner
list: SharedList<list>
map: SharedMap<map>
map: SharedMap<unordered_map>
*/
// Problem: sharedvar.print wird von datatype.element.print() benötigt.
//          sharedavar.type.print  braucht datatype.print()
// 1. print kein element von sharedvar und datatyp, sondern so, dass alles
//        gleichzeitig abgehandelt wird
// 2. print kein element von
// TODO: Avoid cyclic dependency between datatype and Sharedvar
    // when creating curvariable.print() to datatype.print() dependency.
    // 1. make DataPrinter()
    //      - includes var and datatype and leaves both incomplete in header
    //      - if not incomplete: dependency on both in the header
    //
    // to dataprinter, but it would become a superheader
    // (every other must include dataprinter not sharedvar and datatype).
    // weeird, doesn't matter?
    // second option is to make the interface not use both
// TODO: Rename to *-Shared
// TODO: translate all of RPC_bindings.generated.cpp
// TODO: translate all of tree.generated.proto
// TODO: add sharedvarattributes
//
// TODO: make server code use nested classes
    return 0;
}
