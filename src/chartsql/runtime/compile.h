/**
 * This file is part of the "FnordMetric" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _FNORDMETRIC_QUERY_COMPILER_H
#define _FNORDMETRIC_QUERY_COMPILER_H
#include <stdlib.h>
#include <vector>
#include <string>
#include <chartsql/runtime/symboltable.h>
#include <chartsql/runtime/CompiledProgram.h>
#include <chartsql/qtree/ScalarExpressionNode.h>
#include <chartsql/qtree/FieldReferenceNode.h>
#include <chartsql/qtree/BuiltinExpressionNode.h>

using namespace fnord;

namespace csql {
class ASTNode;
class SValue;

class Compiler {
public:
  Compiler(SymbolTable* symbol_table);

  CompiledExpression* compile(ASTNode* ast, size_t* scratchpad_size);

  ScopedPtr<CompiledProgram> compile(RefPtr<ScalarExpressionNode> node);

  SymbolTable* symbolTable() { return symbol_table_; }

protected:

  CompiledExpression* compileScalarExpression(
     RefPtr<ScalarExpressionNode> node,
     size_t* scratchpad_size);

  CompiledExpression* compileSelectList(
      ASTNode* select_list,
      size_t* scratchpad_size);

  CompiledExpression* compileOperator(
      const std::string& name,
      ASTNode* ast,
      size_t* scratchpad_size);

  CompiledExpression* compileLiteral(ASTNode* ast);

  CompiledExpression* compileColumnReference(ASTNode* ast);

  CompiledExpression* compileColumnReference(
      RefPtr<FieldReferenceNode> node);

  CompiledExpression* compileChildren(ASTNode* ast, size_t* scratchpad_size);

  CompiledExpression* compileMethodCall(ASTNode* ast, size_t* scratchpad_size);

  CompiledExpression* compileMethodCall(
      RefPtr<BuiltinExpressionNode> node,
      size_t* scratchpad_size);

  SymbolTable* symbol_table_;
};

}
#endif
