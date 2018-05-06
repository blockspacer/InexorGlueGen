#pragma once

#include <pugiconfig.hpp>
#include <pugixml.hpp>

#include "inexor/filesystem/path.hpp"

#include <string>
#include <vector>
#include <unordered_map>

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
/// There are two types of such xml files: one for source files, one for class definitions.

struct ASTs
{
    typedef inexor::filesystem::Path Path;
    typedef std::unique_ptr<pugi::xml_document> xml_document_ptr;


    /// In case the class xml is a shared option definition, it will be saved in here.
    /// Key: name of the class, value: the class AST xml file.
    std::unordered_map<std::string, xml_document_ptr> option_xmls;

    /// In case the class xml is not a shared option definition, it will be saved in here, together with the refid of the class.
    /// Key: refid of the class, value: the class AST xml file.
    std::unordered_map<std::string, xml_document_ptr> class_xmls;

    /// The xml files containing the ASTs of all source code files.
    std::vector<xml_document_ptr> code_xmls;

    /// Loads the xml files into memory and sorts them to class, code or option_xmls.
    ASTs(const Path &directory);

private:
    bool load_xml_file(const Path &file, xml_document_ptr &xml);
};

} } // ns inexor::gluegen
