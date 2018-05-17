
#include "inexor/gluegen/ASTs.hpp"
#include "inexor/gluegen/render_files.hpp"
#include "inexor/gluegen/SharedAttributes.hpp"
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/SharedVarDatatypes.hpp"

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
    const vector<string> partial_files = cli_config.count("help") ? cli_config["partial_file"].as<vector<string>>() : vector<string>();
    const string xml_AST_folder = cli_config["doxygen_AST_folder"].as<string>();

    ASTs code;
    code.load_from_directory(xml_AST_folder);

    // options definitions need to get parsed before anything else, since you look for them when parsing the vars/functions/classes..
// auto shared_attribute_definitions = parse_shared_attribute_definitions(code.attribute_class_xmls);

    auto shared_var_occurences = find_shared_var_occurences(code.code_xmls);

    auto shared_var_type_names = get_shared_var_types(shared_var_occurences);
    auto shared_var_type_definitions = find_class_definitions(code.class_xmls, shared_var_type_names);

    // add print functions to each type.
    mustache::data template_base_data{mustache::data::type::object};
    template_base_data.set("type_definitions", print_shared_var_type_definitions(shared_var_type_definitions)); //, shared_attribute_definitions));
    template_base_data.set("variables", print_shared_var_occurences(shared_var_occurences));//, shared_attribute_definitions));

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

1. filenames ---
2. data structures in files
- was wird gesucht?
- was ist mit options?---
3. methodennamen + functional_classes (factories?)
4. bugfixen
*/
// Later:
//     Optimization:
//       - if the type is already made accessible, skip it.
//   - take care of saving the real typename when dealing with nested class definitions
//         def x { def y };
//         x::y a;
//         x is not the namespace, but the containing class.
    return 0;
}
