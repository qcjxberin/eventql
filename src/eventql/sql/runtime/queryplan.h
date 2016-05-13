/**
 * This file is part of the "FnordMetric" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <eventql/util/stdtypes.h>
#include <eventql/util/autoref.h>
#include <eventql/sql/qtree/QueryTreeNode.h>
#include <eventql/sql/scheduler.h>
#include <eventql/sql/transaction.h>

namespace csql {
class Runtime;
class ResultList;

class QueryPlan : public RefCounted {
public:

  /**
   * This constructor isn't usually directly called by users but invoked through
   * Runtime::buildQueryPlan
   */
  QueryPlan(
      Transaction* txn,
      Vector<RefPtr<QueryTreeNode>> qtrees);

  /**
   * Execute one of the statements in the query plan. The statement is referenced
   * by index. The index must be in the range  [0, numStatements)
   *
   * This method returns a result cursor. The underlying query will be executed
   * incrementally as result rows are read from the cursor
   */
  ScopedPtr<ResultCursor> execute(size_t stmt_idx);

  /**
   * Execute one of the statements in the query plan. The statement is referenced
   * by index. The index must be in the range  [0, numStatements)
   *
   * This method materializes the full result list into the provided result list
   * object
   */
  void execute(size_t stmt_idx, ResultList* result_list);

  /**
   * Retruns the number of statements in the query plan
   */
  size_t numStatements() const;

  /**
   * Returns the result column list ("header") for a statement in the query plan.
   * The statement is referenced by index. The index must be in the range
   * [0, numStatements)
   */
  const Vector<String>& getStatementOutputColumns(size_t stmt_idx);

  void setScheduler(RefPtr<Scheduler> scheduler);
  RefPtr<QueryTreeNode> getStatement(size_t stmt_idx) const;

  Transaction* getTransaction() const;

  //void onOutputComplete(size_t stmt_idx, Function<void ()> fn);
  //void onOutputRow(size_t stmt_idx, RowSinkFn fn);
  //void onQueryFinished(Function<void ()> fn);

  //void storeResults(size_t stmt_idx, ResultList* result_list);

protected:
  Transaction* txn_;
  Vector<RefPtr<QueryTreeNode>> qtrees_;
  Vector<Vector<String>> statement_columns_;
  RefPtr<Scheduler> scheduler_;
};

}