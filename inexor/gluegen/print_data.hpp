#pragma once

#include "inexor/gluegen/SharedAttributes.hpp"
#include <kainjow/mustache.hpp>

#include <vector>
#include <string>

namespace inexor {
namespace gluegen {

struct SharedVariable;
struct shared_class_definition;

extern kainjow::mustache::data print_data(
        const std::vector<SharedVariable> &shared_var_occurences,
        const std::unordered_map<std::string, shared_class_definition> &type_definitions,
        const std::unordered_map<std::string, attribute_definition> &attribute_definitions);

}
}
