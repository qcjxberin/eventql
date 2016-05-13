/**
 * This file is part of the "libcsql" project
 *   Copyright (c) 2015 Paul Asmuth
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <eventql/sql/expressions/table/nested_loop_join.h>

namespace csql {

NestedLoopJoin::NestedLoopJoin(
    Transaction* txn,
    JoinType join_type,
    const Vector<JoinNode::InputColumnRef>& input_map,
    Vector<ValueExpression> select_expressions,
    Option<ValueExpression> join_cond_expr,
    Option<ValueExpression> where_expr,
    ScopedPtr<TableExpression> base_tbl,
    ScopedPtr<TableExpression> joined_tbl) :
    txn_(txn),
    join_type_(join_type),
    input_map_(input_map),
    input_buf_(input_map.size()),
    select_exprs_(std::move(select_expressions)),
    join_cond_expr_(std::move(join_cond_expr)),
    where_expr_(std::move(where_expr)),
    base_tbl_(std::move(base_tbl)),
    joined_tbl_(std::move(joined_tbl)),
    joined_tbl_pos_(0) {}

static const size_t kMaxInMemoryRows = 1000000;

ScopedPtr<ResultCursor> NestedLoopJoin::execute() {
  auto joined_cursor = joined_tbl_->execute();
  Vector<SValue> row(joined_cursor->getNumColumns());
  while (joined_cursor->next(row.data(), row.size())) {
    joined_tbl_data_.emplace_back(row);

    if (joined_tbl_data_.size() >= kMaxInMemoryRows) {
      RAISE(
          kRuntimeError,
          "Nested Loop JOIN intermediate result set is too large, try using an"
          " equi-join instead.");
    }
  }

  base_tbl_cursor_ = base_tbl_->execute();
  base_tbl_row_.resize(base_tbl_cursor_->getNumColumns());

  switch (join_type_) {
    case JoinType::OUTER:
      return executeOuterJoin();
    case JoinType::INNER:
      if (join_cond_expr_.isEmpty()) {
        /* fallthrough */
      } else {
        return executeInnerJoin();
      }
    case JoinType::CARTESIAN:
      return executeCartesianJoin();
    default:
      RAISE(kIllegalStateError);
  }
}

ScopedPtr<ResultCursor> NestedLoopJoin::executeCartesianJoin() {
  auto cursor = [this] (SValue* row, int row_len) -> bool {
    for (;;) {
      if (joined_tbl_pos_ == 0 || joined_tbl_pos_ == joined_tbl_data_.size()) {
        joined_tbl_pos_ = 0;

        if (!base_tbl_cursor_->next(
              base_tbl_row_.data(),
              base_tbl_row_.size())) {
          return false;
        }
      }

      while (joined_tbl_pos_ < joined_tbl_data_.size()) {
        const auto& joined_table_row = joined_tbl_data_[joined_tbl_pos_++];

        for (size_t i = 0; i < input_map_.size(); ++i) {
          const auto& m = input_map_[i];

          switch (m.table_idx) {
            case 0:
              input_buf_[i] = base_tbl_row_[m.column_idx];
              break;
            case 1:
              input_buf_[i] = joined_table_row[m.column_idx];
              break;
            default:
              RAISE(kRuntimeError, "invalid table index");
          }
        }

        if (!where_expr_.isEmpty()) {
          SValue pred;
          VM::evaluate(
              txn_,
              where_expr_.get().program(),
              input_buf_.size(),
              input_buf_.data(),
              &pred);

          if (!pred.getBool()) {
            continue;
          }
        }

        for (int i = 0; i < select_exprs_.size() && i < row_len; ++i) {
          VM::evaluate(
              txn_,
              select_exprs_[i].program(),
              input_buf_.size(),
              input_buf_.data(),
              &row[i]);
        }

        return true;
      }
    }
  };

  return mkScoped(new DefaultResultCursor( select_exprs_.size(), cursor));
}

ScopedPtr<ResultCursor> NestedLoopJoin::executeInnerJoin() {
  auto cursor = [this] (SValue* row, int row_len) -> bool {
    for (;;) {
      if (joined_tbl_pos_ == 0 || joined_tbl_pos_ == joined_tbl_data_.size()) {
        joined_tbl_pos_ = 0;

        if (!base_tbl_cursor_->next(
              base_tbl_row_.data(),
              base_tbl_row_.size())) {
          return false;
        }
      }

      while (joined_tbl_pos_ < joined_tbl_data_.size()) {
        const auto& joined_table_row = joined_tbl_data_[joined_tbl_pos_++];

        for (size_t i = 0; i < input_map_.size(); ++i) {
          const auto& m = input_map_[i];

          switch (m.table_idx) {
            case 0:
              input_buf_[i] = base_tbl_row_[m.column_idx];
              break;
            case 1:
              input_buf_[i] = joined_table_row[m.column_idx];
              break;
            default:
              RAISE(kRuntimeError, "invalid table index");
          }
        }

        {
          SValue pred;
          VM::evaluate(
              txn_,
              join_cond_expr_.get().program(),
              input_buf_.size(),
              input_buf_.data(),
              &pred);

          if (!pred.getBool()) {
            continue;
          }
        }

        if (!where_expr_.isEmpty()) {
          SValue pred;
          VM::evaluate(
              txn_,
              where_expr_.get().program(),
              input_buf_.size(),
              input_buf_.data(),
              &pred);

          if (!pred.getBool()) {
            continue;
          }
        }

        for (int i = 0; i < select_exprs_.size() && i < row_len; ++i) {
          VM::evaluate(
              txn_,
              select_exprs_[i].program(),
              input_buf_.size(),
              input_buf_.data(),
              &row[i]);
        }

        return true;
      }
    }
  };

  return mkScoped(new DefaultResultCursor( select_exprs_.size(), cursor));
}

ScopedPtr<ResultCursor> NestedLoopJoin::executeOuterJoin() {
  auto cursor = [this] (SValue* row, int row_len) -> bool {
    for (;;) {
      if (joined_tbl_pos_ == 0 ||
          joined_tbl_pos_ == joined_tbl_data_.size()) {
        joined_tbl_pos_ = 0;
        joined_tbl_row_found_ = false;

        if (!base_tbl_cursor_->next(
              base_tbl_row_.data(),
              base_tbl_row_.size())) {
          return false;
        }
      }

      bool match = false;
      while (joined_tbl_pos_ < joined_tbl_data_.size()) {
        const auto& joined_table_row = joined_tbl_data_[joined_tbl_pos_++];

        for (size_t i = 0; i < input_map_.size(); ++i) {
          const auto& m = input_map_[i];

          switch (m.table_idx) {
            case 0:
              input_buf_[i] = base_tbl_row_[m.column_idx];
              break;
            case 1:
              input_buf_[i] = joined_table_row[m.column_idx];
              break;
            default:
              RAISE(kRuntimeError, "invalid table index");
          }
        }

        SValue pred;
        VM::evaluate(
            txn_,
            join_cond_expr_.get().program(),
            input_buf_.size(),
            input_buf_.data(),
            &pred);

        if (!pred.getBool()) {
          continue;
        }

        joined_tbl_row_found_ = true;
        match = true;
        break;
      }

      if (match || !joined_tbl_row_found_) {
        if (!joined_tbl_row_found_) {
          for (size_t i = 0; i < input_map_.size(); ++i) {
            switch (input_map_[i].table_idx) {
              case 0:
                break;
              case 1:
                input_buf_[i] = SValue{};
                break;
              default:
                RAISE(kRuntimeError, "invalid table index");
            }
          }
        }

        if (!where_expr_.isEmpty()) {
          SValue pred;
          VM::evaluate(
              txn_,
              where_expr_.get().program(),
              input_buf_.size(),
              input_buf_.data(),
              &pred);

          if (!pred.getBool()) {
            continue;
          }
        }

        for (int i = 0; i < select_exprs_.size() && i < row_len; ++i) {
          VM::evaluate(
              txn_,
              select_exprs_[i].program(),
              input_buf_.size(),
              input_buf_.data(),
              &row[i]);
        }

        return true;
      }
    }
  };

  return mkScoped(new DefaultResultCursor( select_exprs_.size(), cursor));
}


}