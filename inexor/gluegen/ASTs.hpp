#pragma once

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include "inexor/filesystem/path.hpp"

#include <string>
#include <vector>

namespace inexor {
namespace gluegen {


//the AST looks as follows:
//- doxygen
//    compounddef[kind="class"]
//       compoundname
//       sectiondef[kind="public-attrib"]
//           memberdef[kind="variable"]
//              name
//              initializer ('= "wert"')
//       sectiondef[kind="public-func"]
//           memberdef[kind="function"]
//              name
//              param
//                 type
//                 declname <-
//                 defval

/// Class containing all xml files (and filenames)
///
/// The .xml files contain doxygen generated ASTs (Abstract Syntax Trees)
/// Doxygen knows two types of such xml files: xml-files for source files, xml-files for class definitions.
struct ASTs
{
    typedef inexor::filesystem::Path Path;
    typedef std::unique_ptr<pugi::xml_document> xml_document_ptr;


    /// In case the class xml is a shared option definition, it will be saved in here.
    std::vector<xml_document_ptr> attribute_class_xmls;

    /// In case the class xml is not a shared option definition, it will be saved in here.
    std::vector<xml_document_ptr> class_xmls;

    /// The xml files containing the ASTs of all source code files.
    std::vector<xml_document_ptr> code_xmls;

    /// Loads the xml files into memory and sorts them to class, code or option_xmls.
    void load_from_directory(const Path &directory);

private:
    bool load_xml_file(const Path &file, xml_document_ptr &xml);
};

} } // ns inexor::gluegen
