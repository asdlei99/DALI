// Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DALI_OPERATORS_TRANSPOSE_TRANSPOSE_H_
#define DALI_OPERATORS_TRANSPOSE_TRANSPOSE_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "dali/pipeline/operator/operator.h"
#include "dali/operators/transpose/cutt/cutt.h"

namespace dali {

namespace detail {

template <typename T>
T Permute(const T& in, const std::vector<int>& permutation) {
  T out = in;
  for (size_t i = 0; i < permutation.size(); i++) {
    auto idx = permutation[i];
    out[i] = in[idx];
  }
  return out;
}

}  // namespace detail


template <typename Backend>
class Transpose : public Operator<Backend> {
 public:
  explicit inline Transpose(const OpSpec &spec)
      : Operator<Backend>(spec)
      , perm_(spec.GetRepeatedArgument<int>("perm"))
      , transpose_layout_(spec.GetArgument<bool>("transpose_layout"))
      , output_layout_arg_(spec.GetArgument<TensorLayout>("output_layout")) {
    if (spec.HasArgument("output_layout")) {
      DALI_ENFORCE(!output_layout_arg_.empty(),
        "Providing an empty output layout is not supported");
    }

    auto check_permutation =
      [](std::vector<int> perm) {
        std::sort(perm.begin(), perm.end());
        for (int i = 0; i < static_cast<int>(perm.size()); ++i) {
          if (perm[i] != i) {
            return false;
          }
        }
        return true;
      };

    DALI_ENFORCE(check_permutation(perm_),
      "Invalid permutation: sorted `perm` is not equal to [0, ..., n-1].");
  }

  ~Transpose() override;

  DISABLE_COPY_MOVE_ASSIGN(Transpose);

 protected:
  bool SetupImpl(std::vector<OutputDesc> &output_desc,
                 const workspace_t<Backend> &ws) override {
    const auto &input = ws.template Input<Backend>(0);
    auto in_layout = input.GetLayout();
    auto sample_ndim = input.shape().sample_dim();
    DALI_ENFORCE(in_layout.ndim() == sample_ndim || in_layout.empty());
    output_layout_ = in_layout;
    if (!output_layout_arg_.empty()) {
      DALI_ENFORCE(output_layout_.ndim() == sample_ndim);
      output_layout_ = output_layout_arg_;
    } else if (transpose_layout_ && !in_layout.empty()) {
      output_layout_ = detail::Permute(in_layout, perm_);
    }
    return false;
  }

  void RunImpl(Workspace<Backend> &ws) override;

 private:
  std::vector<int> perm_;
  bool transpose_layout_;
  TensorLayout output_layout_arg_;
  TensorLayout output_layout_;

  cuttHandle cutt_handle_ = 0;
  // used by dense TL cuttHandle
  TensorShape<> previous_iter_shape_;

  USE_OPERATOR_MEMBERS();
  using Operator<Backend>::RunImpl;
};

}  // namespace dali

#endif  // DALI_OPERATORS_TRANSPOSE_TRANSPOSE_H_
