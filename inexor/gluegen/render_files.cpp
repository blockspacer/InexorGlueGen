
#include "inexor/gluegen/render_files.hpp"
#include "inexor/filesystem/path.hpp"

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include <kainjow/mustache.hpp>

#include <fstream>

using namespace pugi;
using namespace kainjow;
using namespace std;

namespace inexor {
namespace gluegen {

/// Render a mustache template into a string.
const std::string render_template(const std::string &tmplate, const std::string &tmplate_filename, const mustache::data &tmpldata)
{
    mustache::mustache tmpl{tmplate};
    if(!tmpl.is_valid())
        std::cout << "Template file " << tmplate_filename << " malformatted: " << tmpl.error_message() << std::endl;
    return tmpl.render(tmpldata);
}

/// Create a file containing the given content.
void save_to_file(const std::string &filepath, const std::string &file_content)
{
    std::ofstream sink{filepath, std::ofstream::trunc};
    sink << file_content;
    std::cout << "Rendering C++ GlueGen file completed (" << filepath << ")" << std::endl;
}

void render_files(mustache::data &tmpldata, const std::vector<std::string> &partial_files, const std::vector<std::string> &template_files)
{
    /*
    for(const string &file : partial_files)
    {
        // 1. Load xml file
        // 2. find field with entries and iterate over them
        for(entry : file_xml.entries){
            // 3. find out how to add a new field here
            template_base_data[entry.key] = render_template(entry.value, template_base_data);
        }
    }
    */
    for(const string &file : template_files)
    {
        std::unique_ptr<xml_document> xml;
        if(!xml->load_file(file.c_str(), parse_default|parse_trim_pcdata))
        {
            std::cout << "XML file representing the AST couldn't be parsed: " << file << std::endl;
            return;
        }
        xml->child("template").value();
        const string gen_file_content = render_template("nix", "nix", "nix"); //xml->child("template").value(), template_base_data);
        save_to_file(xml->child("filename").value(), gen_file_content);
    }
}

}
}
