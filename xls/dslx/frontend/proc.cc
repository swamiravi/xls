// Copyright 2023 The XLS Authors
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

#include "xls/dslx/frontend/proc.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "xls/common/indent.h"
#include "xls/common/logging/logging.h"
#include "xls/dslx/frontend/ast.h"
#include "xls/dslx/frontend/pos.h"

namespace xls::dslx {

absl::StatusOr<ProcStmt> ToProcStmt(AstNode* n) {
  if (auto* s = dynamic_cast<Function*>(n)) {
    return s;
  }
  if (auto* s = dynamic_cast<ProcMember*>(n)) {
    return s;
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Node is not a valid ProcStmt; type: ", n->GetNodeTypeName()));
}

// -- class Proc

Proc::Proc(Module* owner, Span span, NameDef* name_def,
           std::vector<ParametricBinding*> parametric_bindings, ProcBody body,
           bool is_public)
    : AstNode(owner),
      span_(std::move(span)),
      name_def_(name_def),
      parametric_bindings_(std::move(parametric_bindings)),
      body_(std::move(body)),
      is_public_(is_public) {
  CHECK(body_.config != nullptr);
  CHECK(body_.next != nullptr);
  CHECK(body_.init != nullptr);
}

Proc::~Proc() = default;

std::vector<AstNode*> Proc::GetChildren(bool want_types) const {
  std::vector<AstNode*> results = {name_def()};
  for (ParametricBinding* pb : parametric_bindings_) {
    results.push_back(pb);
  }
  for (ProcMember* p : body_.members) {
    results.push_back(p);
  }
  results.push_back(body_.config);
  results.push_back(body_.next);
  results.push_back(body_.init);
  return results;
}

std::string Proc::ToString() const {
  std::string pub_str = is_public() ? "pub " : "";
  std::string parametric_str;
  if (!parametric_bindings().empty()) {
    parametric_str = absl::StrFormat(
        "<%s>",
        absl::StrJoin(
            parametric_bindings(), ", ",
            [](std::string* out, ParametricBinding* parametric_binding) {
              absl::StrAppend(out, parametric_binding->ToString());
            }));
  }
  std::string members_str = absl::StrJoin(
      members(), "\n", [](std::string* out, const ProcMember* member) {
        out->append(absl::StrCat(member->ToString(), ";"));
      });
  if (!members().empty()) {
    members_str.append("\n");
  }

  // Init functions are special, since they shouldn't be printed with
  // parentheses (since they can't take args).
  std::string init_str = Indent(
      absl::StrCat("init ", init().body()->ToString()), kRustSpacesPerIndent);

  constexpr std::string_view kTemplate = R"(%sproc %s%s {
%s%s
%s
%s
})";
  return absl::StrFormat(
      kTemplate, pub_str, name_def()->identifier(), parametric_str,
      Indent(members_str, kRustSpacesPerIndent),
      Indent(config().ToUndecoratedString("config"), kRustSpacesPerIndent),
      init_str,
      Indent(next().ToUndecoratedString("next"), kRustSpacesPerIndent));
}

// -- class TestProc

TestProc::~TestProc() = default;

std::string TestProc::ToString() const {
  return absl::StrFormat("#[test_proc]\n%s", proc_->ToString());
}

// -- class ProcMember

ProcMember::ProcMember(Module* owner, NameDef* name_def,
                       TypeAnnotation* type_annotation)
    : AstNode(owner),
      name_def_(name_def),
      type_annotation_(type_annotation),
      span_(name_def_->span().start(), type_annotation_->span().limit()) {}

ProcMember::~ProcMember() = default;

}  // namespace xls::dslx
