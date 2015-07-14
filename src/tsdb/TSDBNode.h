/**
 * This file is part of the "libfnord" project
 *   Copyright (c) 2015 Paul Asmuth
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _FNORD_TSDB_TSDBNODE_H
#define _FNORD_TSDB_TSDBNODE_H
#include <fnord/stdtypes.h>
#include <fnord/random.h>
#include <fnord/option.h>
#include <fnord/thread/queue.h>
#include <fnord/mdb/MDB.h>
#include <tsdb/TableConfig.pb.h>
#include <tsdb/Partition.h>
#include <tsdb/TSDBNodeRef.h>
#include <tsdb/CompactionWorker.h>
#include <tsdb/ReplicationWorker.h>
#include <tsdb/TSDBNodeConfig.pb.h>
#include <tsdb/TSDBTableInfo.h>
#include <tsdb/SQLEngine.h>

using namespace fnord;

namespace tsdb {

class TSDBNode {
public:

  TSDBNode(
      const String& db_path,
      RefPtr<dproc::ReplicationScheme> replication_scheme,
      http::HTTPConnectionPool* http);

  void configure(const TSDBNodeConfig& config, const String& base_path);

  TableConfig* configFor(
      const String& tsdb_namespace,
      const String& stream_key) const;

  Option<RefPtr<Partition>> findPartition(
      const String& tsdb_namespace,
      const String& stream_key,
      const SHA1Hash& partition_key);

  RefPtr<Partition> findOrCreatePartition(
      const String& tsdb_namespace,
      const String& stream_key,
      const SHA1Hash& partition_key);

  void listTables(
      Function<void (const TSDBTableInfo& table)> fn) const;

  Option<TSDBTableInfo> tableInfo(
      const String& tsdb_namespace,
      const String& table_key) const;

  const String& dbPath() const;

  void start(
      size_t num_comaction_threads = 8,
      size_t num_replication_threads = 4);

  void stop();

protected:

  void reopenPartitions();

  TSDBNodeRef noderef_;
  Vector<Pair<String, ScopedPtr<TableConfig>>> configs_;
  std::mutex mutex_;
  HashMap<String, RefPtr<Partition>> partitions_;
  Vector<RefPtr<CompactionWorker>> compaction_workers_;
  Vector<RefPtr<ReplicationWorker>> replication_workers_;
  msg::MessageSchemaRepository schemas_;
};

} // namespace tdsb

#endif
