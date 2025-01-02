// Copyright 2024 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "xls/dslx/frontend/zip_ast.h"

#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "xls/common/status/status_macros.h"
#include "xls/dslx/frontend/ast.h"
#include "xls/dslx/frontend/ast_node_visitor_with_default.h"

namespace xls::dslx {
namespace {

// Returns true if `node` is of type `T`; false otherwise.
template <typename T>
bool MatchType(const AstNode* node) {
  return dynamic_cast<const T*>(node) != nullptr;
}

// A visitor intended to be invoked for both the LHS and RHS trees, to perform
// the work of `ZipAst` non-recursively. The caller must do the recursive
// descent itself, and invoke this visitor for each LHS/RHS counterpart node
// encountered, with the LHS first.
class ZipVisitor : public AstNodeVisitorWithDefault {
 public:
  ZipVisitor(AstNodeVisitor* lhs_visitor, AstNodeVisitor* rhs_visitor,
             absl::AnyInvocable<absl::Status(const AstNode*, const AstNode*)>
                 accept_mismatch_callback)
      : lhs_visitor_(lhs_visitor),
        rhs_visitor_(rhs_visitor),
        accept_mismatch_callback_(std::move(accept_mismatch_callback)) {}

  absl::AnyInvocable<absl::Status(const AstNode*, const AstNode*)>&
  accept_mismatch_callback() {
    return accept_mismatch_callback_;
  }

#define DECLARE_HANDLER(__type)                           \
  absl::Status Handle##__type(const __type* n) override { \
    return Handle<__type>(n);                             \
  }
  XLS_DSLX_AST_NODE_EACH(DECLARE_HANDLER)
#undef DECLARE_HANDLER

 private:
  // Handles what may be the LHS or RHS of a given node pair. We assume each LHS
  // comes first.
  template <typename T>
  absl::Status Handle(const T* node) {
    if (!lhs_.has_value()) {
      lhs_ = node;
      match_fn_ = MatchType<T>;
      return absl::OkStatus();
    }
    // `node` is the RHS if we get here.
    if (match_fn_(node)) {
      XLS_RETURN_IF_ERROR((*lhs_)->Accept(lhs_visitor_));
      XLS_RETURN_IF_ERROR(node->Accept(rhs_visitor_));
    } else {
      XLS_RETURN_IF_ERROR(accept_mismatch_callback_(*lhs_, node));
    }
    lhs_ = std::nullopt;
    match_fn_ = nullptr;
    return absl::OkStatus();
  }

  AstNodeVisitor* lhs_visitor_;
  AstNodeVisitor* rhs_visitor_;
  absl::AnyInvocable<absl::Status(const AstNode*, const AstNode*)>
      accept_mismatch_callback_;

  std::optional<const AstNode*> lhs_;
  absl::AnyInvocable<bool(const AstNode*)> match_fn_ = nullptr;
};

// Helper for `ZipAst` which runs recursively and invokes the same `visitor` for
// two whole subtrees.
absl::Status ZipInternal(ZipVisitor* visitor, const AstNode* lhs,
                         const AstNode* rhs) {
  XLS_RETURN_IF_ERROR(lhs->Accept(visitor));
  XLS_RETURN_IF_ERROR(rhs->Accept(visitor));
  std::vector<AstNode*> lhs_children = lhs->GetChildren(/*want_types=*/true);
  std::vector<AstNode*> rhs_children = rhs->GetChildren(/*want_types=*/true);
  if (lhs_children.size() != rhs_children.size()) {
    return visitor->accept_mismatch_callback()(lhs, rhs);
  }
  for (int i = 0; i < lhs_children.size(); i++) {
    XLS_RETURN_IF_ERROR(ZipInternal(visitor, lhs_children[i], rhs_children[i]));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status ZipAst(
    const AstNode* lhs, const AstNode* rhs, AstNodeVisitor* lhs_visitor,
    AstNodeVisitor* rhs_visitor,
    absl::AnyInvocable<absl::Status(const AstNode*, const AstNode*)>
        accept_mismatch_callback) {
  ZipVisitor visitor(lhs_visitor, rhs_visitor,
                     std::move(accept_mismatch_callback));
  return ZipInternal(&visitor, lhs, rhs);
}

}  // namespace xls::dslx