#include "codegen.hpp"


void codegen_inline_visitor::visit(std::shared_ptr<cross_join_op> op) {
    pipelined = true;
    jids.push_back(query_id_inline);
    
    std::vector<int> types;
    get_rhs_type(op->get_rhs(), types);

    BasicBlock *join_lhs_entry = BasicBlock::Create(ctx.getContext(), "entry_join_lhs", main_function);
    BasicBlock *consume = BasicBlock::Create(ctx.getContext(), "consume", main_function);
    BasicBlock *concat_qrl = BasicBlock::Create(ctx.getContext(), "concat_qrl", main_function);
    BasicBlock *end = BasicBlock::Create(ctx.getContext(), "end", main_function);

    auto get_join_tp_at = ctx.extern_func("get_join_tp_at");
    auto node_reg = ctx.extern_func("get_node_res_at");
    auto rship_reg = ctx.extern_func("get_rship_res_at");
    auto get_size = ctx.extern_func("get_mat_res_size");
    auto int_to_reg = ctx.extern_func("int_to_reg");

    // link with previous operator
    link_operator(join_lhs_entry);

    auto cur_idx = ctx.getBuilder().CreateAlloca(ctx.int64Ty);
    ctx.getBuilder().CreateStore(ConstantInt::get(ctx.int64Ty, 0), cur_idx);

    auto opid = ConstantInt::get(ctx.int64Ty, op->operator_id_);
    auto qctx_f = main_function->args().begin() + 1;

    auto joiner_arg = ctx.getBuilder().CreateBitCast(
        ctx.getBuilder().CreateLoad(ctx.getBuilder().CreateInBoundsGEP(qctx_f, {ctx.LLVM_ZERO, opid})),
        ctx.int8PtrTy);

    Value *tp;
    auto jid = ConstantInt::get(ctx.int64Ty, query_id_inline);
    auto ma = ctx.getBuilder().CreateCall(get_size, {joiner_arg, jid});
    ctx.getBuilder().CreateStore(ma, max_idx);
    query_id_inline++;

    auto loop_body = ctx.while_loop_condition(main_function, cur_idx, max_idx, PContext::WHILE_COND::LT, end,
                                            [&](BasicBlock *body, BasicBlock *epilog) {
                                                // get current index for join vector

                                                auto idx = ctx.getBuilder().CreateLoad(cur_idx);
                                                tp = ctx.getBuilder().CreateCall(get_join_tp_at, {joiner_arg, jid, idx});

                                                ctx.getBuilder().CreateBr(concat_qrl);
                                            });        

    ctx.getBuilder().SetInsertPoint(concat_qrl);

    for(auto i = 0u; i < types.size(); i++) {
        auto idx = ConstantInt::get(ctx.int64Ty, i);
        if(types.at(i) == 0) {
            auto n = ctx.getBuilder().CreateCall(node_reg, {tp, idx});
            reg_query_results.push_back({n, 0});
        } else if(types.at(i) == 1) {
            auto r = ctx.getBuilder().CreateCall(rship_reg, {tp, idx});
            reg_query_results.push_back({r, 1});
        } else if(types.at(i) == 2) {
            auto i = ctx.getBuilder().CreateCall(int_to_reg, {tp, idx});
            auto i_alloc = insert_alloca(ctx.int64Ty);
            ctx.getBuilder().CreateStore(i, i_alloc);
            reg_query_results.push_back({i_alloc, 2});
        } 
    }

    auto i = ctx.getBuilder().CreateLoad(cur_idx);
    auto i_add = ctx.getBuilder().CreateAdd(i, ctx.LLVM_ONE);
    ctx.getBuilder().CreateStore(i_add, cur_idx);
    ctx.getBuilder().CreateBr(consume);

    ctx.getBuilder().SetInsertPoint(end);
    ctx.getBuilder().CreateBr(main_return);

    ctx.getBuilder().SetInsertPoint(consume);
    
    prev_bb = consume;
    main_return = loop_body;
}