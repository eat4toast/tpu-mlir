//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Top/IR/TopOps.h"
#include "tpu_mlir/Support/Helper/Module.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace tpu_mlir::helper;
using namespace tpu_mlir::top;
using namespace tpu_mlir::trait;

struct MergeSliceOp : public OpRewritePattern<SliceOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(SliceOp op,
                                PatternRewriter &rewriter) const override {

    auto in_op = op.input().getDefiningOp();
    if (!isa<SliceOp>(in_op) || in_op->hasOneUse() == false) {
      return failure();
    }
    auto output_shape = Module::getShape(op.output());
    auto num_dims = output_shape.size();
    auto in_slice = cast<SliceOp>(in_op);
    auto cur_offset = Module::getI64Array(op.offset());
    auto cur_steps = Module::getI64Array(op.steps());
    auto in_offset = Module::getI64Array(in_slice.offset());
    auto in_steps = Module::getI64Array(in_slice.steps());

    std::vector<int64_t> new_offset(num_dims, 0);
    std::vector<int64_t> new_steps(num_dims, 1);
    for (int i =0; i < num_dims; i++) {
        auto cur_off = cur_offset->at(i);
        auto cur_s = cur_steps->at(i);
        assert(cur_s > 0);
        auto in_off = in_offset->at(i);
        auto in_s = in_steps->at(i);
        assert(in_s > 0);
        new_offset[i] = in_off + cur_off * in_s;
        new_steps[i] = in_s * cur_s;
    }
    op->setAttr("offset", rewriter.getI64ArrayAttr(new_offset));
    op->setAttr("steps", rewriter.getI64ArrayAttr(new_steps));
    op->setOperand(0, in_slice.input());
    in_op->erase();
    return success();
  }
};

void SliceOp::getCanonicalizationPatterns(RewritePatternSet &results,
                                          MLIRContext *context) {
  results.insert<MergeSliceOp>(context);
}
