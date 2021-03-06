/*!
*  Copyright (c) 2017 by Contributors
* \file cuda/injective.h
* \brief CUDA schedule for injective operations
*/
#ifndef TOPI_CUDA_SOFTMAX_H_
#define TOPI_CUDA_SOFTMAX_H_

#include "topi/tags.h"
#include "topi/detail/fuse.h"
#include "tvm/tvm.h"
#include "tvm/build_module.h"

namespace topi {
using namespace tvm;

namespace cuda {

/*!
 * \brief Create a CUDA schedule for the given softmax output tensors.
 *
 * \param target The target to generate a schedule for.
 * \param outs The output tensors.
 *
 * \return A schedule for the given ops.
 */
inline Schedule schedule_softmax(const Target &target, const Array<Tensor>& outs) {
  Array<Operation> out_ops;
  for (auto t : outs) {
    out_ops.push_back(t->op);
  }
  auto s = create_schedule(out_ops);

  auto softmax = outs[0];
  auto max_elem = softmax->op->InputTensors()[1];
  auto expsum = softmax->op->InputTensors()[2];

  int num_thread = 64;
  auto block_x = tvm::thread_axis(Range(), "blockIdx.x");
  auto thread_x = tvm::thread_axis(Range(0, num_thread), "threadIdx.x");

  s[max_elem].bind(max_elem->op.as<ComputeOpNode>()->axis[0], block_x);

  auto k = expsum->op.as<ComputeOpNode>()->reduce_axis[0];
  IterVar ko, ki;
  s[expsum].split(k, num_thread, &ko, &ki);
  auto EF = s.rfactor(expsum, ki)[0];
  s[expsum].bind(s[expsum]->op.as<ComputeOpNode>()->axis[0], block_x);
  s[expsum].bind(s[expsum]->op.as<ComputeOpNode>()->reduce_axis[0], thread_x);
  s[EF].compute_at(s[expsum], s[expsum]->op.as<ComputeOpNode>()->reduce_axis[0]);
  s[expsum].set_store_predicate(thread_x->var == 0);

  IterVar tx, xi;
  s[softmax].split_by_nparts(softmax->op.as<ComputeOpNode>()->axis[1], num_thread, &tx, &xi);
  s[softmax].bind(tx, thread_x);

  return s;
}

}  // namespace cuda
}  // namespace topi
#endif  // TOPI_CUDA_SOFTMAX_H_
