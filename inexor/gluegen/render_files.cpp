
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

/// Create a file containing the given content.
void save_to_file(const std::string &filepath, const std::string &file_content)
{
    std::ofstream sink{filepath, std::ofstream::trunc};
    sink << file_content;
    std::cout << "Rendering C++ GlueGen file completed (" << filepath << ")" << std::endl;
}

void render_files(mustache::data &tmpldata,
                  const std::vector<std::string> &partial_files,
                  const std::vector<std::string> &template_files)
{
    for(const string &file : template_files)
    {
        auto xml = make_unique<xml_document>();
        pugi::xml_parse_result result = xml->load_file(file.c_str(), parse_default|parse_trim_pcdata);
        if(!result)
        {
            std::cout << "XML file defining the rendering template couldn't be parsed: " << file << "\n"
            << result.description() << std::endl;
            return;
        }
        // template data is just for this file, since we are adding partials
        mustache::data local_tmpldata(tmpldata);

        // firstly add all defined partials, each adding its contents to the template data
        // the content will be executed in place.
        for (auto &a : xml->children("partial"))
        {
            const string partial_name = a.attribute("name").value();
            const string partial_templ = a.child_value();
            mustache::mustache tmpl{partial_templ};
            if(!tmpl.is_valid())
                std::cout << "Error in template file (" << file << "). Malformatted partial (" << partial_name << "):\n"
                            << tmpl.error_message() << std::endl;
            kainjow::mustache::partial partial_value([partial_templ]() {
                return partial_templ;
            });
            local_tmpldata.set(partial_name, partial_value);
        }

        // secondly render all files, using the templatedata
        for(auto &a : xml->children("file"))
        {
            const string file_name = a.attribute("filename").value();
            const string file_templ = a.child_value();
            mustache::mustache tmpl{file_templ};
            if(!tmpl.is_valid())
                std::cout << "Error in template file (" << file << "). Malformatted file content (" << file_name << "):\n"
                            << tmpl.error_message() << std::endl;

            const string file_content = tmpl.render(local_tmpldata);
            save_to_file(file_name, file_content);
        }
    }
}

}
}
