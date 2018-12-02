#ifndef TENSORFLOW_COMPILER_XLA_RPC_XRT_SESSION_H_
#define TENSORFLOW_COMPILER_XLA_RPC_XRT_SESSION_H_

#include <functional>
#include <map>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/framework/ops.h"
#include "tensorflow/cc/framework/scope.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/xla_client/debug_macros.h"

namespace xla {

// Encapsulates an XRT session and its associated node cache. XrtSession are not
// thread safe, but are always once byt one thread at a time. The
// XrtSessionCache will keep creating new sessions if not enough are available
// to satisfy the threads requests.
class XrtSession {
 public:
  // A cached node captures that single node, or the mini-graph root node,
  // together with the place-holders necessary to feed the node/sub-graph.
  // The end-point node can be either a tensorflow Operation or an Output.
  struct CachedNode {
    CachedNode(tensorflow::Output output,
               std::vector<tensorflow::ops::Placeholder> holders)
        : output(std::move(output)), holders(std::move(holders)) {}
    CachedNode(tensorflow::Operation operation,
               std::vector<tensorflow::ops::Placeholder> holders)
        : operation(std::move(operation)), holders(std::move(holders)) {}

    absl::optional<tensorflow::Output> output;
    absl::optional<tensorflow::Operation> operation;
    std::vector<tensorflow::ops::Placeholder> holders;
  };

  // The node cache holds a set of CachedNode of the same kind (by the means of
  // the NodeTypes entries).
  // The NodeCache access is not thread safe, but so is XrtSession.
  class NodeCache {
   public:
    bool Empty() const { return position_ >= nodes_.size(); }

    const CachedNode& Get() {
      XLA_CHECK_LT(position_, nodes_.size());
      ++position_;
      return *nodes_[position_ - 1];
    }

    void Add(std::shared_ptr<CachedNode> node) {
      nodes_.push_back(std::move(node));
    }

    void Rewind() { position_ = 0; }

   private:
    std::vector<std::shared_ptr<CachedNode>> nodes_;
    size_t position_ = 0;
  };

  explicit XrtSession(const tensorflow::SessionOptions& session_options);

  const string& target() const { return target_; }

  tensorflow::Scope* root() { return &root_; }

  tensorflow::ClientSession* session() { return &session_; }

  NodeCache* GetNodeCache(const string& key) { return &node_cache_[key]; }

  void Reset();

  static string GetCacheKey(const string& op_name, const string& device);

 private:
  string target_;
  tensorflow::Scope root_;
  tensorflow::ClientSession session_;
  std::map<string, NodeCache> node_cache_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_RPC_XRT_SESSION_H_
