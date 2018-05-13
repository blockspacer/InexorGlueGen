#pragma once

#include <kainjow/mustache.hpp>

#include <vector>
#include <string>

namespace inexor {
namespace gluegen {

    /// We load the xml files which are containing definitions of mustache partials, or the filename plus the mustache
    /// template for a file we want to generate using the templatedata given.
    extern void render_files(kainjow::mustache::data &tmpldata, const std::vector<std::string> &partial_files,
                             const std::vector<std::string> &template_files);
}
}
