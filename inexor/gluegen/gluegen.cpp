#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>

#include <boost/program_options.hpp>

#include "inexor/gluegen/tree.hpp"
#include "inexor/gluegen/parse_sourcecode.hpp"
#include "inexor/gluegen/generate_files.hpp"
#include "inexor/gluegen/fill_templatedata.hpp"
#include "inexor/gluegen/ParserContext.hpp"


using namespace inexor::rpc::gluegen;
namespace po = boost::program_options;
using std::string;
using std::vector;

void usage(const std::string &ex, const po::options_description &params) {
    std::cerr
        << "Inexor GlueGen       Codegenerator which takes an AST produced by Doxygen, finds marked variables \"
                "and generates arbitrary code based on it.\n\n\n\"
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
        ("partial_file", po::value<std::vector<std::string>>()->multitoken()->composing()->required(),
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

    const vector<string> &template_files = cli_config["template_file"].as<vector<string>>();
    const vector<string> &partial_files = cli_config["partial_file"].as<vector<string>>();
    const string &xml_AST_file = cli_config["doxygen_AST_folder"].as<string>();

    // Read the list of variables

    ParserContext parserctx;

    find_shared_decls(xml_AST_file, parserctx); // fill the tree vector

    TemplateData templdata = fill_templatedata(parserctx, ns_str);

    // Write the protoc file
    render_proto_file(proto_file, proto_template, templdata);

    // Write cpp files
    render_cpp_tree_data(cpp_file, cpp_template, templdata);

    return 0;
}
