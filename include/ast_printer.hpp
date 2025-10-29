#pragma once

#include <string>

#include "ast.hpp"

// Return a human-readable dump of the AST
std::string dump_ast(const Program& program);
