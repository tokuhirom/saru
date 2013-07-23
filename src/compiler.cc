/* vim:ts=2:sw=2:tw=0:
 */

extern "C" {
#include "moarvm.h"
}
#include "compiler.h"

uint16_t Kiji_compiler_if_op(KijiCompiler* self, uint16_t cond_reg) {
    switch (Kiji_compiler_get_local_type(self, cond_reg)) {
    case MVM_reg_int64:
        return MVM_OP_if_i;
    case MVM_reg_num64:
    return MVM_OP_if_n;
    case MVM_reg_str:
    return MVM_OP_if_s;
    case MVM_reg_obj:
    return MVM_OP_if_o;
    default:
    abort();
    }
}

    KIJI_STATIC_INLINE void Kiji_compiler_pop_frame(KijiCompiler* self) {
      self->frames_.pop_back();
    }

    int Kiji_compiler_push_frame(KijiCompiler* self, const std::string & name) {
      MVMThreadContext *tc_ = self->tc_;
      MVMCompUnit *cu_ = self->cu_;
      assert(tc_);
      char *buf = (char*)malloc((name.size()+32)*sizeof(char));
      int len = snprintf(buf, name.size()+31, "%s%d", name.c_str(), self->frame_no_++);
      // TODO Newxz
      KijiFrame* frame = (KijiFrame*)malloc(sizeof(KijiFrame));
      memset(frame, 0, sizeof(KijiFrame));
      frame->frame.name = MVM_string_utf8_decode(tc_, tc_->instance->VMString, buf, len);
      free(buf);
      if (self->frames_.size() != 0) {
        Kiji_frame_set_outer(frame, self->frames_.back());
      }
      self->frames_.push_back(frame);
      cu_->num_frames++;
      Renew(cu_->frames, cu_->num_frames, MVMStaticFrame*);
      cu_->frames[cu_->num_frames-1] = &(self->frames_.back()->frame);
      cu_->frames[cu_->num_frames-1]->cu = cu_;
      cu_->frames[cu_->num_frames-1]->work_size = 0;
      return cu_->num_frames-1;
    }

  void KijiLabel::put() {
    assert(address_ == -1);
    address_ = compiler_->frames_.back()->frame.bytecode_size;

    // rewrite reserved addresses
    for (auto r: reserved_addresses_) {
      Kiji_asm_write_uint32_t_for(&(*(compiler_->frames_.back())), address_, r);
    }
    reserved_addresses_.empty();
  }

      // fixme: `put` is not the best verb in English here.
      void KijiLoopGuard::put_last() {
        last_offset_ = compiler_->ASM_BYTECODE_SIZE()-1+1;
      }
      void KijiLoopGuard::put_redo() {
        redo_offset_ = compiler_->ASM_BYTECODE_SIZE()-1+1;
      }
      void KijiLoopGuard::put_next() {
        next_offset_ = compiler_->ASM_BYTECODE_SIZE()-1+1;
      }
KijiLoopGuard::~KijiLoopGuard() {
        MVMuint32 end_offset = compiler_->ASM_BYTECODE_SIZE()-1;

        MVMFrameHandler *last_handler = new MVMFrameHandler;
        last_handler->start_offset = start_offset_;
        last_handler->end_offset = end_offset;
        last_handler->category_mask = MVM_EX_CAT_LAST;
        last_handler->action = MVM_EX_ACTION_GOTO;
        last_handler->block_reg = 0;
        last_handler->goto_offset = last_offset_;
        Kiji_frame_push_handler(compiler_->frames_.back(), last_handler);

        MVMFrameHandler *next_handler = new MVMFrameHandler;
        next_handler->start_offset = start_offset_;
        next_handler->end_offset = end_offset;
        next_handler->category_mask = MVM_EX_CAT_NEXT;
        next_handler->action = MVM_EX_ACTION_GOTO;
        next_handler->block_reg = 0;
        next_handler->goto_offset = next_offset_;
        Kiji_frame_push_handler(compiler_->frames_.back(), next_handler);

        MVMFrameHandler *redo_handler = new MVMFrameHandler;
        redo_handler->start_offset = start_offset_;
        redo_handler->end_offset = end_offset;
        redo_handler->category_mask = MVM_EX_CAT_REDO;
        redo_handler->action = MVM_EX_ACTION_GOTO;
        redo_handler->block_reg = 0;
        redo_handler->goto_offset = redo_offset_;
        Kiji_frame_push_handler(compiler_->frames_.back(), redo_handler);
      }
      KijiLoopGuard::KijiLoopGuard(KijiCompiler *compiler) :compiler_(compiler) {
        start_offset_ = compiler_->ASM_BYTECODE_SIZE()-1;
      }
    void KijiCompiler::compile_array(uint16_t array_reg, const PVIPNode* node) {
      if (node->type==PVIP_NODE_LIST) {
        for (int i=0; i<node->children.size; i++) {
          PVIPNode* m = node->children.nodes[i];
          compile_array(array_reg, m);
        }
      } else {
        auto reg = this->to_o(do_compile(node));
        ASM_PUSH_O(array_reg, reg);
      }
    }
    int KijiCompiler::do_compile(const PVIPNode*node) {
      // printf("node: %s\n", node.type_name());
      switch (node->type) {
      case PVIP_NODE_POSTDEC: { // $i--
        assert(node->children.size == 1);
        if (node->children.nodes[0]->type != PVIP_NODE_VARIABLE) {
          MVM_panic(MVM_exitcode_compunit, "The argument for postinc operator is not a variable");
        }
        auto reg_no = get_variable(node->children.nodes[0]->pv);
        auto i_tmp = to_i(reg_no);
        ASM_DEC_I(i_tmp);
        set_variable(node->children.nodes[0]->pv, to_o(i_tmp));
        return reg_no;
      }
      case PVIP_NODE_POSTINC: { // $i++
        assert(node->children.size == 1);
        if (node->children.nodes[0]->type != PVIP_NODE_VARIABLE) {
          MVM_panic(MVM_exitcode_compunit, "The argument for postinc operator is not a variable");
        }
        auto reg_no = get_variable(node->children.nodes[0]->pv);
        auto i_tmp = to_i(reg_no);
        ASM_INC_I(i_tmp);
        auto dst_reg = to_o(i_tmp);
        set_variable(node->children.nodes[0]->pv, dst_reg);
        return reg_no;
      }
      case PVIP_NODE_PREINC: { // ++$i
        assert(node->children.size == 1);
        if (node->children.nodes[0]->type != PVIP_NODE_VARIABLE) {
          MVM_panic(MVM_exitcode_compunit, "The argument for postinc operator is not a variable");
        }
        auto reg_no = get_variable(node->children.nodes[0]->pv);
        auto i_tmp = to_i(reg_no);
        ASM_INC_I(i_tmp);
        auto dst_reg = to_o(i_tmp);
        set_variable(node->children.nodes[0]->pv, dst_reg);
        return dst_reg;
      }
      case PVIP_NODE_PREDEC: { // --$i
        assert(node->children.size == 1);
        if (node->children.nodes[0]->type != PVIP_NODE_VARIABLE) {
          MVM_panic(MVM_exitcode_compunit, "The argument for postinc operator is not a variable");
        }
        auto reg_no = get_variable(PVIPSTRING2STDSTRING(node->children.nodes[0]->pv));
        auto i_tmp = to_i(reg_no);
        ASM_DEC_I(i_tmp);
        auto dst_reg = to_o(i_tmp);
        set_variable(PVIPSTRING2STDSTRING(node->children.nodes[0]->pv), dst_reg);
        return dst_reg;
      }
      case PVIP_NODE_UNARY_BITWISE_NEGATION: { // +^1
        auto reg = to_i(do_compile(node->children.nodes[0]));
        ASM_BNOT_I(reg, reg);
        return reg;
      }
      case PVIP_NODE_BRSHIFT: { // +>
        auto l = to_i(do_compile(node->children.nodes[0]));
        auto r = to_i(do_compile(node->children.nodes[1]));
        ASM_BRSHIFT_I(r, l, r);
        return r;
      }
      case PVIP_NODE_BLSHIFT: { // +<
        auto l = to_i(do_compile(node->children.nodes[0]));
        auto r = to_i(do_compile(node->children.nodes[1]));
        ASM_BLSHIFT_I(r, l, r);
        return r;
      }
      case PVIP_NODE_ABS: {
        // TODO support abs_n?
        auto r = to_i(do_compile(node->children.nodes[0]));
        ASM_ABS_I(r, r);
        return r;
      }
      case PVIP_NODE_LAST: {
        // break from for, while, loop.
        auto ret = REG_OBJ();
        ASM_THROWCATLEX(
          ret,
          MVM_EX_CAT_LAST
        );
        return ret;
      }
      case PVIP_NODE_REDO: {
        // redo from for, while, loop.
        auto ret = REG_OBJ();
        ASM_THROWCATLEX(
          ret,
          MVM_EX_CAT_REDO
        );
        return ret;
      }
      case PVIP_NODE_NEXT: {
        // continue from for, while, loop.
        auto ret = REG_OBJ();
        ASM_THROWCATLEX(
          ret,
          MVM_EX_CAT_NEXT
        );
        return ret;
      }
      case PVIP_NODE_RETURN: {
        assert(node->children.size ==1);
        auto reg = do_compile(node->children.nodes[0]);
        if (reg < 0) {
          MEMORY_ERROR();
        }
        // ASM_RETURN_O(this->to_o(reg));
        switch (Kiji_compiler_get_local_type(this, reg)) {
        case MVM_reg_int64:
          ASM_RETURN_I(reg);
          break;
        case MVM_reg_str:
          ASM_RETURN_S(reg);
          break;
        case MVM_reg_obj:
          ASM_RETURN_O(this->to_o(reg));
          break;
        case MVM_reg_num64:
          ASM_RETURN_N(reg);
          break;
        default:
          MVM_panic(MVM_exitcode_compunit, "Compilation error. Unknown register for returning: %d", Kiji_compiler_get_local_type(this, reg));
        }
        return UNKNOWN_REG;
      }
      case PVIP_NODE_MODULE: {
        // nop, for now.
        return UNKNOWN_REG;
      }
      case PVIP_NODE_USE: {
        assert(node->children.size==1);
        auto name = PVIPSTRING2STDSTRING(node->children.nodes[0]->pv);
        if (name == "v6") {
          return UNKNOWN_REG; // nop.
        }
        std::string path = std::string("lib/") + name + ".p6";
        FILE *fp = fopen(path.c_str(), "rb");
        if (!fp) {
          MVM_panic(MVM_exitcode_compunit, "Cannot open file: %s", path.c_str());
        }
        PVIPString *error;
        PVIPNode* root_node = PVIP_parse_fp(fp, 0, &error);
        if (!root_node) {
          MVM_panic(MVM_exitcode_compunit, "Cannot parse: %s", error->buf);
        }

        auto frame_no = Kiji_compiler_push_frame(this, path);
        ASM_CHECKARITY(0,0);
        this->do_compile(root_node);
        ASM_RETURN();
        Kiji_compiler_pop_frame(this);

        auto code_reg = REG_OBJ();
        ASM_GETCODE(code_reg, frame_no);
        MVMCallsite* callsite = new MVMCallsite;
        memset(callsite, 0, sizeof(MVMCallsite));
        callsite->arg_count = 0;
        callsite->num_pos = 0;
        callsite->arg_flags = NULL;
        auto callsite_no = push_callsite(callsite);
        ASM_PREPARGS(callsite_no);
        ASM_INVOKE_V(code_reg);

        return UNKNOWN_REG;
      }
      case PVIP_NODE_DIE: {
        int msg_reg = to_s(do_compile(node->children.nodes[0]));
        int dst_reg = REG_OBJ();
        assert(msg_reg != UNKNOWN_REG);
        ASM_DIE(dst_reg, msg_reg);
        return UNKNOWN_REG;
      }
      case PVIP_NODE_WHILE: {
        /*
         *  label_while:
         *    cond
         *    unless_o label_end
         *    body
         *  label_end:
         */

        KijiLoopGuard loop(this);

        auto label_while = label();
        loop.put_next();
          int reg = do_compile(node->children.nodes[0]);
          assert(reg != UNKNOWN_REG);
          auto label_end = label_unsolved();
          unless_any(reg, label_end);
          do_compile(node->children.nodes[1]);
          goto_(label_while);
        label_end.put();

        loop.put_last();

        return UNKNOWN_REG;
      }
      case PVIP_NODE_LAMBDA: {
        auto frame_no = Kiji_compiler_push_frame(this, "lambda");
        ASM_CHECKARITY(
          node->children.nodes[0]->children.size,
          node->children.nodes[0]->children.size
        );
        for (int i=0; i<node->children.nodes[0]->children.size; i++) {
          PVIPNode *n = node->children.nodes[0]->children.nodes[i];
          int reg = REG_OBJ();
          int lex = push_lexical(PVIPSTRING2STDSTRING(n->pv), MVM_reg_obj);
          ASM_PARAM_RP_O(reg, i);
          ASM_BINDLEX(lex, 0, reg);
        }
        auto retval = do_compile(node->children.nodes[1]);
        if (retval == UNKNOWN_REG) {
          retval = REG_OBJ();
          ASM_NULL(retval);
        }
        return_any(retval);
        Kiji_compiler_pop_frame(this);

        // warn if void context.
        auto dst_reg = REG_OBJ();
        ASM_GETCODE(dst_reg, frame_no);

        return dst_reg;
      }
      case PVIP_NODE_BLOCK: {
        auto frame_no = Kiji_compiler_push_frame(this, "block");
        ASM_CHECKARITY(0,0);
        for (int i=0; i<node->children.size; i++) {
          PVIPNode *n = node->children.nodes[i];
          (void)do_compile(n);
        }
        ASM_RETURN();
        Kiji_compiler_pop_frame(this);

        auto frame_reg = REG_OBJ();
        ASM_GETCODE(frame_reg, frame_no);

        MVMCallsite* callsite = new MVMCallsite;
        memset(callsite, 0, sizeof(MVMCallsite));
        callsite->arg_count = 0;
        callsite->num_pos = 0;
        callsite->arg_flags = NULL;
        auto callsite_no = push_callsite(callsite);
        ASM_PREPARGS(callsite_no);

        ASM_INVOKE_V(frame_reg); // trash result

        return UNKNOWN_REG;
      }
      case PVIP_NODE_STRING: {
        int str_num = push_string(node->pv);
        int reg_num = REG_STR();
        ASM_CONST_S(reg_num, str_num);
        return reg_num;
      }
      case PVIP_NODE_INT: {
        uint16_t reg_num = REG_INT64();
        int64_t n = node->iv;
        ASM_CONST_I64(reg_num, n);
        return reg_num;
      }
      case PVIP_NODE_NUMBER: {
        uint16_t reg_num = REG_NUM64();
        MVMnum64 n = node->nv;
        ASM_CONST_N64(reg_num, n);
        return reg_num;
      }
      case PVIP_NODE_BIND: {
        auto lhs = node->children.nodes[0];
        auto rhs = node->children.nodes[1];
        if (lhs->type == PVIP_NODE_MY) {
          // my $var := foo;
          int lex_no = do_compile(lhs);
          int val    = this->to_o(do_compile(rhs));
          ASM_BINDLEX(
            lex_no, // lex number
            0,      // frame outer count
            val     // value
          );
          return val;
        } else if (lhs->type == PVIP_NODE_OUR) {
          // our $var := foo;
          assert(lhs->children.nodes[0]->type == PVIP_NODE_VARIABLE);
          push_pkg_var(PVIPSTRING2STDSTRING(lhs->children.nodes[0]->pv));
          auto varname = push_string(lhs->children.nodes[0]->pv);
          int val    = to_o(do_compile(rhs));
          int outer = 0;
          int lex_no = 0;
          if (!find_lexical_by_name("$?PACKAGE", &lex_no, &outer)) {
            MVM_panic(MVM_exitcode_compunit, "Unknown lexical variable in find_lexical_by_name: %s\n", "$?PACKAGE");
          }
          auto package = REG_OBJ();
          ASM_GETLEX(
            package,
            lex_no,
            outer // outer frame
          );
          // TODO getwho
          auto varname_s = REG_STR();
          ASM_CONST_S(varname_s, varname);
          // 0x0F    bindkey_o           r(obj) r(str) r(obj)
          ASM_BINDKEY_O(
            package,
            varname_s,
            val
          );
          return val;
        } else if (lhs->type == PVIP_NODE_VARIABLE) {
          // $var := foo;
          int val    = this->to_o(do_compile(rhs));
          set_variable(std::string(lhs->pv->buf, lhs->pv->len), val);
          return val;
        } else {
          printf("You can't bind value to %s, currently.\n", PVIP_node_name(lhs->type));
          abort();
        }
      }
      case PVIP_NODE_METHOD: {
        PVIPNode * name_node = node->children.nodes[0];
        std::string name(name_node->pv->buf, name_node->pv->len);

        auto funcreg = REG_OBJ();
        auto funclex = push_lexical(std::string("&") + name, MVM_reg_obj);
        auto func_pos = ASM_BYTECODE_SIZE() + 2 + 2;
        ASM_GETCODE(funcreg, 0);
        ASM_BINDLEX(
            funclex,
            0, // frame outer count
            funcreg
        );

        // Compile function body
        auto frame_no = Kiji_compiler_push_frame(this, name);

        // TODO process named params
        // TODO process types
        {
          ASM_CHECKARITY(
              node->children.nodes[1]->children.size+1,
              node->children.nodes[1]->children.size+1
          );

          {
            // push self
            int lex = push_lexical("__self", MVM_reg_obj);
            int reg = REG_OBJ();
            ASM_PARAM_RP_O(reg, 0);
            ASM_BINDLEX(lex, 0, reg);
          }

          for (int i=1; i<node->children.nodes[1]->children.size; ++i) {
            auto n = node->children.nodes[1]->children.nodes[i];
            int reg = REG_OBJ();
            int lex = push_lexical(n->pv, MVM_reg_obj);
            ASM_PARAM_RP_O(reg, i);
            ASM_BINDLEX(lex, 0, reg);
            ++i;
          }
        }

        bool returned = false;
        {
          const PVIPNode*stmts = node->children.nodes[2];
          assert(stmts->type == PVIP_NODE_STATEMENTS);
          for (int i=0; i<stmts->children.size ; ++i) {
            PVIPNode * n=stmts->children.nodes[i];
            int reg = do_compile(n);
            if (i==stmts->children.size-1 && reg >= 0) {
              switch (Kiji_compiler_get_local_type(this, reg)) {
              case MVM_reg_int64:
                ASM_RETURN_I(reg);
                break;
              case MVM_reg_str:
                ASM_RETURN_S(reg);
                break;
              case MVM_reg_obj:
                ASM_RETURN_O(reg);
                break;
              case MVM_reg_num64:
                ASM_RETURN_N(reg);
                break;
              default: abort();
              }
              returned = true;
            }
          }

          // return null
          if (!returned) {
            int reg = REG_OBJ();
            ASM_NULL(reg);
            ASM_RETURN_O(reg);
          }
        }
        Kiji_compiler_pop_frame(this);

        ASM_WRITE_UINT16_T(frame_no, func_pos);

        // bind method object to class how
        if (!current_class_how_) {
          MVM_panic(MVM_exitcode_compunit, "Compilation error. You can't write methods outside of class definition");
        }
        {
          MVMString * methname = MVM_string_utf8_decode(tc_, tc_->instance->VMString, name.c_str(), name.size());
          // self, type_obj, name, method
          MVMObject * method_table = ((MVMKnowHOWREPR *)current_class_how_)->body.methods;
          MVMObject* code_type = tc_->instance->boot_types->BOOTCode;
          MVMCode *coderef = (MVMCode*)REPR(code_type)->allocate(tc_, STABLE(code_type));
          coderef->body.sf = this->cu_->frames[frame_no];
          REPR(method_table)->ass_funcs->bind_key_boxed(tc_, STABLE(method_table),
              method_table, OBJECT_BODY(method_table), (MVMObject *)methname, (MVMObject*)coderef);
        }

        return funcreg;
      }
      case PVIP_NODE_FUNC: {
        PVIPNode * name_node = node->children.nodes[0];
        std::string name(name_node->pv->buf, name_node->pv->len);

        auto funcreg = REG_OBJ();
        auto funclex = push_lexical(std::string("&") + name, MVM_reg_obj);
        auto func_pos = ASM_BYTECODE_SIZE() + 2 + 2;
        ASM_GETCODE(funcreg, 0);
        ASM_BINDLEX(
            funclex,
            0, // frame outer count
            funcreg
        );

        // Compile function body
        auto frame_no = Kiji_compiler_push_frame(this, name);

        // TODO process named params
        // TODO process types
        {
          ASM_CHECKARITY(
              node->children.nodes[1]->children.size,
              node->children.nodes[1]->children.size
          );

          for (int i=0; i<node->children.nodes[1]->children.size; ++i) {
            auto n = node->children.nodes[1]->children.nodes[i];
            int reg = REG_OBJ();
            int lex = push_lexical(n->pv, MVM_reg_obj);
            ASM_PARAM_RP_O(reg, i);
            ASM_BINDLEX(lex, 0, reg);
            ++i;
          }
        }

        bool returned = false;
        {
          const PVIPNode*stmts = node->children.nodes[2];
          assert(stmts->type == PVIP_NODE_STATEMENTS);
          for (int i=0; i<stmts->children.size ; ++i) {
            PVIPNode * n=stmts->children.nodes[i];
            int reg = do_compile(n);
            if (i==stmts->children.size-1 && reg >= 0) {
              switch (Kiji_compiler_get_local_type(this, reg)) {
              case MVM_reg_int64:
                ASM_RETURN_I(reg);
                break;
              case MVM_reg_str:
                ASM_RETURN_S(reg);
                break;
              case MVM_reg_obj:
                ASM_RETURN_O(reg);
                break;
              case MVM_reg_num64:
                ASM_RETURN_N(reg);
                break;
              default: abort();
              }
              returned = true;
            }
          }

          // return null
          if (!returned) {
            int reg = REG_OBJ();
            ASM_NULL(reg);
            ASM_RETURN_O(reg);
          }
        }
        Kiji_compiler_pop_frame(this);

        ASM_WRITE_UINT16_T(frame_no, func_pos);

        return funcreg;
      }
      case PVIP_NODE_VARIABLE: {
        // copy lexical variable to register
        return get_variable(std::string(node->pv->buf, node->pv->len));
      }
      case PVIP_NODE_CLARGS: { // @*ARGS
        auto retval = REG_OBJ();
        ASM_WVAL(retval, 0,0);
        return retval;
      }
      case PVIP_NODE_CLASS: {
        return compile_class(node);
      }
      case PVIP_NODE_FOR: {
        //   init_iter
        // label_for:
        //   body
        //   shift iter
        //   if_o label_for
        // label_end:

        KijiLoopGuard loop(this);
          auto src_reg = to_o(do_compile(node->children.nodes[0]));
          auto iter_reg = REG_OBJ();
          auto label_end = label_unsolved();
          ASM_ITER(iter_reg, src_reg);
          unless_any(iter_reg, label_end);

        auto label_for = label();
        loop.put_next();

          auto val = REG_OBJ();
          ASM_SHIFT_O(val, iter_reg);

          if (node->children.nodes[1]->type == PVIP_NODE_LAMBDA) {
            auto body = do_compile(node->children.nodes[1]);
            MVMCallsite* callsite = new MVMCallsite;
            memset(callsite, 0, sizeof(MVMCallsite));
            callsite->arg_count = 1;
            callsite->num_pos = 1;
            callsite->arg_flags = new MVMCallsiteEntry[1];
            callsite->arg_flags[0] = MVM_CALLSITE_ARG_OBJ;
            auto callsite_no = push_callsite(callsite);
            ASM_PREPARGS(callsite_no);
            ASM_ARG_O(0, val);
            ASM_INVOKE_V(body);
          } else {
            int it = push_lexical("$_", MVM_reg_obj);
            ASM_BINDLEX(it, 0, val);
            do_compile(node->children.nodes[1]);
          }

          if_any(iter_reg, label_for);

        label_end.put();
        loop.put_last();

        return UNKNOWN_REG;
      }
      case PVIP_NODE_OUR: {
        if (node->children.size != 1) {
          printf("NOT IMPLEMENTED\n");
          abort();
        }
        auto n = node->children.nodes[0];
        if (n->type != PVIP_NODE_VARIABLE) {
          printf("This is variable: %s\n", PVIP_node_name(n->type));
          exit(0);
        }
        push_pkg_var(std::string(n->pv->buf, n->pv->len));
        return UNKNOWN_REG;
      }
      case PVIP_NODE_MY: {
        if (node->children.size != 1) {
          printf("NOT IMPLEMENTED\n");
          abort();
        }
        auto n = node->children.nodes[0];
        if (n->type != PVIP_NODE_VARIABLE) {
          printf("This is variable: %s\n", PVIP_node_name(n->type));
          exit(0);
        }
        int idx = push_lexical(n->pv, MVM_reg_obj);
        return idx;
      }
      case PVIP_NODE_UNLESS: {
        //   cond
        //   if_o label_end
        //   body
        // label_end:
        auto cond_reg = do_compile(node->children.nodes[0]);
        auto label_end = label_unsolved();
        if_any(cond_reg, label_end);
        auto dst_reg = do_compile(node->children.nodes[1]);
        label_end.put();
        return dst_reg;
      }
      case PVIP_NODE_IF: {
        //   if_cond
        //   if_o if_cond, label_if
        //   elsif1_cond
        //   if_o elsif1_cond, label_elsif1
        //   elsif2_cond
        //   if_o elsif2_cond, label_elsif2
        // label_else:
        //   else_body
        //   goto label_end
        // label_if:
        //   if_body
        //   goto lable_end
        // label_elsif1:
        //   elsif2_body
        //   goto lable_end
        // label_elsif2:
        //   elsif1_body
        //   goto lable_end
        // label_end:

        auto if_cond_reg = do_compile(node->children.nodes[0]);
        auto if_body = node->children.nodes[1];
        auto dst_reg = REG_OBJ();

        auto label_if = label_unsolved();
        if_any(if_cond_reg, label_if);

        // put else if conditions
        std::list<KijiLabel> elsif_poses;
        for (int i=2; i<node->children.size; ++i) {
          PVIPNode *n = node->children.nodes[i];
          if (n->type == PVIP_NODE_ELSE) {
            break;
          }
          auto reg = do_compile(n->children.nodes[0]);
          elsif_poses.emplace_back(this);
          if_any(reg, elsif_poses.back());
        }

        // compile else clause
        if (node->children.nodes[node->children.size-1]->type == PVIP_NODE_ELSE) {
          compile_statements(node->children.nodes[node->children.size-1], dst_reg);
        }

        auto label_end = label_unsolved();
        goto_(label_end);

        // update if_label and compile if body
        label_if.put();
          compile_statements(if_body, dst_reg);
          goto_(label_end);

        // compile elsif body
        for (int i=2; i<node->children.size; i++) {
          PVIPNode*n = node->children.nodes[i];
          if (n->type == PVIP_NODE_ELSE) {
            break;
          }

          elsif_poses.front().put();
          elsif_poses.pop_front();
          compile_statements(n->children.nodes[1], dst_reg);
          goto_(label_end);
        }
        assert(elsif_poses.size() == 0);

        label_end.put();

        return dst_reg;
      }
      case PVIP_NODE_ELSIF:
      case PVIP_NODE_ELSE: {
        abort();
      }
      case PVIP_NODE_IDENT: {
        int lex_no, outer;
        // auto lex_no = find_lexical_by_name(std::string(node->pv->buf, node->pv->len), outer);
        // class Foo { }; Foo;
        if (find_lexical_by_name(std::string(node->pv->buf, node->pv->len), &lex_no, &outer)) {
          auto reg_no = REG_OBJ();
          ASM_GETLEX(
            reg_no,
            lex_no,
            outer // outer frame
          );
          return reg_no;
        // sub fooo { }; foooo;
        } else if (find_lexical_by_name(std::string("&") + std::string(node->pv->buf, node->pv->len), &lex_no, &outer)) {
          auto func_reg_no = REG_OBJ();
          ASM_GETLEX(
            func_reg_no,
            lex_no,
            outer // outer frame
          );

          MVMCallsite* callsite = new MVMCallsite;
          memset(callsite, 0, sizeof(MVMCallsite));
          callsite->arg_count = 0;
          callsite->num_pos = 0;
          callsite->arg_flags = new MVMCallsiteEntry[0];

          auto callsite_no = push_callsite(callsite);
          ASM_PREPARGS(callsite_no);

          auto dest_reg = REG_OBJ(); // ctx
          ASM_INVOKE_O(
              dest_reg,
              func_reg_no
          );
          return dest_reg;
        } else {
          MVM_panic(MVM_exitcode_compunit, "Unknown lexical variable in find_lexical_by_name: %s\n", std::string(node->pv->buf, node->pv->len).c_str());
        }
      }
      case PVIP_NODE_STATEMENTS:
        for (int i=0; i<node->children.size; i++) {
          PVIPNode*n = node->children.nodes[i];
          // should i return values?
          do_compile(n);
        }
        return UNKNOWN_REG;
      case PVIP_NODE_STRING_CONCAT: {
        auto dst_reg = REG_STR();
        auto lhs = node->children.nodes[0];
        auto rhs = node->children.nodes[1];
        auto l = to_s(do_compile(lhs));
        auto r = to_s(do_compile(rhs));
        ASM_CONCAT_S(
          dst_reg,
          l,
          r
        );
        return dst_reg;
      }
      case PVIP_NODE_REPEAT_S: { // x operator
        auto dst_reg = REG_STR();
        auto lhs = node->children.nodes[0];
        auto rhs = node->children.nodes[1];
        ASM_REPEAT_S(
          dst_reg,
          to_s(do_compile(lhs)),
          to_i(do_compile(rhs))
        );
        return dst_reg;
      }
      case PVIP_NODE_LIST:
      case PVIP_NODE_ARRAY: { // TODO: use 6model's container feature after released it.
        // create array
        auto array_reg = REG_OBJ();
        ASM_HLLLIST(array_reg);
        ASM_CREATE(array_reg, array_reg);

        // push elements
        for (int i=0; i<node->children.size; i++) {
          PVIPNode*n = node->children.nodes[i];
          compile_array(array_reg, n);
        }
        return array_reg;
      }
      case PVIP_NODE_ATPOS: {
        assert(node->children.size == 2);
        auto container = do_compile(node->children.nodes[0]);
        auto idx       = this->to_i(do_compile(node->children.nodes[1]));
        auto dst = REG_OBJ();
        ASM_ATPOS_O(dst, container, idx);
        return dst;
      }
      case PVIP_NODE_IT_METHODCALL: {
        PVIPNode * node_it = PVIP_node_new_string(PVIP_NODE_VARIABLE, "$_", 2);
        PVIPNode * call = PVIP_node_new_children2(PVIP_NODE_METHODCALL, node_it, node->children.nodes[0]);
        if (node->children.size == 2) {
          PVIP_node_push_child(call, node->children.nodes[1]);
        }
        // TODO possibly memory leaks
        return do_compile(call);
      }
      case PVIP_NODE_METHODCALL: {
        assert(node->children.size == 3 || node->children.size==2);
        auto obj = to_o(do_compile(node->children.nodes[0]));
        auto str = push_string(node->children.nodes[1]->pv);
        auto meth = REG_OBJ();
        auto ret = REG_OBJ();

        ASM_FINDMETH(meth, obj, str);
        ASM_ARG_O(0, obj);

        if (node->children.size == 3) {
          auto args = node->children.nodes[2];

          MVMCallsite* callsite = new MVMCallsite;
          memset(callsite, 0, sizeof(MVMCallsite));
          callsite->arg_count = 1+args->children.size;
          callsite->num_pos = 1+args->children.size;
          callsite->arg_flags = new MVMCallsiteEntry[1+args->children.size];
          callsite->arg_flags[0] = MVM_CALLSITE_ARG_OBJ;
          int i=1;
          for (int j=0; j<args->children.size; j++) {
            PVIPNode* a= args->children.nodes[j];
            callsite->arg_flags[i] = MVM_CALLSITE_ARG_OBJ;
            ASM_ARG_O(i, to_o(do_compile(a)));
            ++i;
          }
          auto callsite_no = push_callsite(callsite);
          ASM_PREPARGS(callsite_no);
        } else {
          MVMCallsite* callsite = new MVMCallsite;
          memset(callsite, 0, sizeof(MVMCallsite));
          callsite->arg_count = 1;
          callsite->num_pos = 1;
          callsite->arg_flags = new MVMCallsiteEntry[1];
          callsite->arg_flags[0] = MVM_CALLSITE_ARG_OBJ;
          auto callsite_no = push_callsite(callsite);
          ASM_PREPARGS(callsite_no);
        }

        ASM_INVOKE_O(ret, meth);
        return ret;
      }
      case PVIP_NODE_CONDITIONAL: {
        /*
         *   cond
         *   unless_o cond, :label_else
         *   if_body
         *   copy dst_reg, result
         *   goto :label_end
         * label_else:
         *   else_bdoy
         *   copy dst_reg, result
         * label_end:
         */
        auto label_end  = label_unsolved();
        auto label_else = label_unsolved();
        auto dst_reg = REG_OBJ();

          auto cond_reg = do_compile(node->children.nodes[0]);
          unless_any(cond_reg, label_else);

          auto if_reg = do_compile(node->children.nodes[1]);
          ASM_SET(dst_reg, to_o(if_reg));
          goto_(label_end);

        label_else.put();
          auto else_reg = do_compile(node->children.nodes[2]);
          ASM_SET(dst_reg, to_o(else_reg));

        label_end.put();

        return dst_reg;
      }
      case PVIP_NODE_NOT: {
        auto src_reg = this->to_i(do_compile(node->children.nodes[0]));
        auto dst_reg = REG_INT64();
        ASM_NOT_I(dst_reg, src_reg);
        return dst_reg;
      }
      case PVIP_NODE_BIN_AND: {
        return this->binary_binop(node, MVM_OP_band_i);
      }
      case PVIP_NODE_BIN_OR: {
        return this->binary_binop(node, MVM_OP_bor_i);
      }
      case PVIP_NODE_BIN_XOR: {
        return this->binary_binop(node, MVM_OP_bxor_i);
      }
      case PVIP_NODE_MUL: {
        return this->numeric_binop(node, MVM_OP_mul_i, MVM_OP_mul_n);
      }
      case PVIP_NODE_SUB: {
        return this->numeric_binop(node, MVM_OP_sub_i, MVM_OP_sub_n);
      }
      case PVIP_NODE_DIV: {
        return this->numeric_binop(node, MVM_OP_div_i, MVM_OP_div_n);
      }
      case PVIP_NODE_ADD: {
        return this->numeric_binop(node, MVM_OP_add_i, MVM_OP_add_n);
      }
      case PVIP_NODE_MOD: {
        return this->numeric_binop(node, MVM_OP_mod_i, MVM_OP_mod_n);
      }
      case PVIP_NODE_POW: {
        return this->numeric_binop(node, MVM_OP_pow_i, MVM_OP_pow_n);
      }
      case PVIP_NODE_INPLACE_ADD: {
        return this->numeric_inplace(node, MVM_OP_add_i, MVM_OP_add_n);
      }
      case PVIP_NODE_INPLACE_SUB: {
        return this->numeric_inplace(node, MVM_OP_sub_i, MVM_OP_sub_n);
      }
      case PVIP_NODE_INPLACE_MUL: {
        return this->numeric_inplace(node, MVM_OP_mul_i, MVM_OP_mul_n);
      }
      case PVIP_NODE_INPLACE_DIV: {
        return this->numeric_inplace(node, MVM_OP_div_i, MVM_OP_div_n);
      }
      case PVIP_NODE_INPLACE_POW: { // **=
        return this->numeric_inplace(node, MVM_OP_pow_i, MVM_OP_pow_n);
      }
      case PVIP_NODE_INPLACE_MOD: { // %=
        return this->numeric_inplace(node, MVM_OP_mod_i, MVM_OP_mod_n);
      }
      case PVIP_NODE_INPLACE_BIN_OR: { // +|=
        return this->binary_inplace(node, MVM_OP_bor_i);
      }
      case PVIP_NODE_INPLACE_BIN_AND: { // +&=
        return this->binary_inplace(node, MVM_OP_band_i);
      }
      case PVIP_NODE_INPLACE_BIN_XOR: { // +^=
        return this->binary_inplace(node, MVM_OP_bxor_i);
      }
      case PVIP_NODE_INPLACE_BLSHIFT: { // +<=
        return this->binary_inplace(node, MVM_OP_blshift_i);
      }
      case PVIP_NODE_INPLACE_BRSHIFT: { // +>=
        return this->binary_inplace(node, MVM_OP_brshift_i);
      }
      case PVIP_NODE_INPLACE_CONCAT_S: { // ~=
        return this->str_inplace(node, MVM_OP_concat_s, MVM_reg_str);
      }
      case PVIP_NODE_INPLACE_REPEAT_S: { // x=
        return this->str_inplace(node, MVM_OP_repeat_s, MVM_reg_int64);
      }
      case PVIP_NODE_UNARY_TILDE: { // ~
        return to_s(do_compile(node->children.nodes[0]));
      }
      case PVIP_NODE_NOP:
        return -1;
      case PVIP_NODE_ATKEY: {
        assert(node->children.size == 2);
        auto dst       = REG_OBJ();
        auto container = to_o(do_compile(node->children.nodes[0]));
        auto key       = to_s(do_compile(node->children.nodes[1]));
        ASM_ATKEY_O(dst, container, key);
        return dst;
      }
      case PVIP_NODE_HASH: {
        auto hashtype = REG_OBJ();
        auto hash     = REG_OBJ();
        ASM_HLLHASH(hashtype);
        ASM_CREATE(hash, hashtype);
        for (int i=0; i<node->children.size; i++) {
          PVIPNode* pair = node->children.nodes[i];
          assert(pair->type == PVIP_NODE_PAIR);
          auto k = to_s(do_compile(pair->children.nodes[0]));
          auto v = to_o(do_compile(pair->children.nodes[1]));
          ASM_BINDKEY_O(hash, k, v);
        }
        return hash;
      }
      case PVIP_NODE_LOGICAL_XOR: { // '^^'
        //   calc_arg1
        //   calc_arg2
        //   if_o arg1, label_a1_true
        //   # arg1 is false.
        //   unless_o arg2, label_both_false
        //   # arg1=false, arg2=true
        //   set dst_reg, arg2
        //   goto label_end
        // label_both_false:
        //   null dst_reg
        //   goto label_end
        // label_a1_true:
        //   if_o arg2, label_both_true
        //   set dst_reg, arg1
        //   goto label_end
        // label_both_true:
        //   set dst_reg, arg1
        //   goto label_end
        // label_end:
        auto label_both_false = label_unsolved();
        auto label_a1_true    = label_unsolved();
        auto label_both_true  = label_unsolved();
        auto label_end        = label_unsolved();

        auto dst_reg = REG_OBJ();

          auto arg1 = to_o(do_compile(node->children.nodes[0]));
          auto arg2 = to_o(do_compile(node->children.nodes[1]));
          if_any(arg1, label_a1_true);
          unless_any(arg2, label_both_false);
          ASM_SET(dst_reg, arg2);
          goto_(label_end);
        label_both_false.put();
          ASM_SET(dst_reg, arg1);
          goto_(label_end);
        label_a1_true.put(); // a1:true, a2:unknown
          if_any(arg2, label_both_true);
          ASM_SET(dst_reg, arg1);
          goto_(label_end);
        label_both_true.put();
          ASM_NULL(dst_reg);
          goto_(label_end);
        label_end.put();
        return dst_reg;
      }
      case PVIP_NODE_LOGICAL_OR: { // '||'
        //   calc_arg1
        //   if_o arg1, label_a1
        //   calc_arg2
        //   set dst_reg, arg2
        //   goto label_end
        // label_a1:
        //   set dst_reg, arg1
        //   goto label_end // omit-able
        // label_end:
        auto label_end = label_unsolved();
        auto label_a1  = label_unsolved();
        auto dst_reg = REG_OBJ();
          auto arg1 = to_o(do_compile(node->children.nodes[0]));
          if_any(arg1, label_a1);
          auto arg2 = to_o(do_compile(node->children.nodes[1]));
          ASM_SET(dst_reg, arg2);
          goto_(label_end);
        label_a1.put();
          ASM_SET(dst_reg, arg1);
        label_end.put();
        return dst_reg;
      }
      case PVIP_NODE_LOGICAL_AND: { // '&&'
        //   calc_arg1
        //   unless_o arg1, label_a1
        //   calc_arg2
        //   set dst_reg, arg2
        //   goto label_end
        // label_a1:
        //   set dst_reg, arg1
        //   goto label_end // omit-able
        // label_end:
        auto label_end = label_unsolved();
        auto label_a1  = label_unsolved();
        auto dst_reg = REG_OBJ();
          auto arg1 = to_o(do_compile(node->children.nodes[0]));
          unless_any(arg1, label_a1);
          auto arg2 = to_o(do_compile(node->children.nodes[1]));
          ASM_SET(dst_reg, arg2);
          goto_(label_end);
        label_a1.put();
          ASM_SET(dst_reg, arg1);
        label_end.put();
        return dst_reg;
      }
      case PVIP_NODE_UNARY_PLUS: {
        return to_n(do_compile(node->children.nodes[0]));
      }
      case PVIP_NODE_UNARY_MINUS: {
        auto reg = do_compile(node->children.nodes[0]);
        if (Kiji_compiler_get_local_type(this, reg) == MVM_reg_int64) {
          ASM_NEG_I(reg, reg);
          return reg;
        } else {
          reg = to_n(reg);
          ASM_NEG_N(reg, reg);
          return reg;
        }
      }
      case PVIP_NODE_CHAIN:
        if (node->children.size==1) {
          return do_compile(node->children.nodes[0]);
        } else {
          return this->compile_chained_comparisions(node);
        }
      case PVIP_NODE_FUNCALL: {
        assert(node->children.size == 2);
        const PVIPNode*ident = node->children.nodes[0];
        const PVIPNode*args  = node->children.nodes[1];
        assert(args->type == PVIP_NODE_ARGS);
        
        if (node->children.nodes[0]->type == PVIP_NODE_IDENT) {
          if (std::string(ident->pv->buf, ident->pv->len) == "say") {
            for (int i=0; i<args->children.size; i++) {
              PVIPNode* a = args->children.nodes[i];
              uint16_t reg_num = to_s(do_compile(a));
              if (i==args->children.size-1) {
                ASM_SAY(reg_num);
              } else {
                ASM_PRINT(reg_num);
              }
            }
            return const_true();
          } else if (std::string(ident->pv->buf, ident->pv->len) == "print") {
            for (int i=0; i<args->children.size; i++) {
              PVIPNode* a = args->children.nodes[i];
              uint16_t reg_num = to_s(do_compile(a));
              ASM_PRINT(reg_num);
            }
            return const_true();
          } else if (std::string(ident->pv->buf, ident->pv->len) == "open") {
            // TODO support arguments
            assert(args->children.size == 1);
            auto fname_s = do_compile(args->children.nodes[0]);
            auto dst_reg_o = REG_OBJ();
            // TODO support latin1, etc.
            auto mode = push_string("r");
            auto mode_s = REG_STR();
            ASM_CONST_S(mode_s, mode);
            ASM_OPEN_FH(dst_reg_o, fname_s, mode_s);
            return dst_reg_o;
          } else if (std::string(ident->pv->buf, ident->pv->len) == "slurp") {
            assert(args->children.size <= 2);
            assert(args->children.size != 2 && "Encoding option is not supported yet");
            auto fname_s = do_compile(args->children.nodes[0]);
            auto dst_reg_s = REG_STR();
            auto encoding_s = REG_STR();
            ASM_CONST_S(encoding_s, push_string("utf8")); // TODO support latin1, etc.
            ASM_SLURP(dst_reg_s, fname_s, encoding_s);
            return dst_reg_s;
          }
        }

        {
          uint16_t func_reg_no;
          if (node->children.nodes[0]->type == PVIP_NODE_IDENT) {
            func_reg_no = REG_OBJ();
            int lex_no, outer;
            if (!find_lexical_by_name(std::string("&") + std::string(ident->pv->buf, ident->pv->len), &lex_no, &outer)) {
              MVM_panic(MVM_exitcode_compunit, "Unknown lexical variable in find_lexical_by_name: %s\n", (std::string("&") + std::string(ident->pv->buf, ident->pv->len)).c_str());
            }
            ASM_GETLEX(
              func_reg_no,
              lex_no,
              outer // outer frame
            );
          } else {
            func_reg_no = to_o(do_compile(node->children.nodes[0]));
          }

          {
            MVMCallsite* callsite = new MVMCallsite;
            memset(callsite, 0, sizeof(MVMCallsite));
            // TODO support named params
            callsite->arg_count = args->children.size;
            callsite->num_pos   = args->children.size;
            callsite->arg_flags = new MVMCallsiteEntry[args->children.size];

            std::vector<uint16_t> arg_regs;

            for (int i=0; i<args->children.size; i++) {
              PVIPNode *a = args->children.nodes[i];
              auto reg = do_compile(a);
              if (reg<0) {
                MVM_panic(MVM_exitcode_compunit, "Compilation error. You should not pass void function as an argument: %s", PVIP_node_name(a->type));
              }

              switch (Kiji_compiler_get_local_type(this, reg)) {
              case MVM_reg_int64:
                callsite->arg_flags[i] = MVM_CALLSITE_ARG_INT;
                arg_regs.push_back(reg);
                break;
              case MVM_reg_num64:
                callsite->arg_flags[i] = MVM_CALLSITE_ARG_NUM;
                arg_regs.push_back(reg);
                break;
              case MVM_reg_str:
                callsite->arg_flags[i] = MVM_CALLSITE_ARG_STR;
                arg_regs.push_back(reg);
                break;
              case MVM_reg_obj:
                callsite->arg_flags[i] = MVM_CALLSITE_ARG_OBJ;
                arg_regs.push_back(reg);
                break;
              default:
                abort();
              }
            }

            auto callsite_no = push_callsite(callsite);
            ASM_PREPARGS(callsite_no);

            int i=0;
            for (auto reg:arg_regs) {
              switch (Kiji_compiler_get_local_type(this, reg)) {
              case MVM_reg_int64:
                ASM_ARG_I(i, reg);
                break;
              case MVM_reg_num64:
                ASM_ARG_N(i, reg);
                break;
              case MVM_reg_str:
                ASM_ARG_S(i, reg);
                break;
              case MVM_reg_obj:
                ASM_ARG_O(i, reg);
                break;
              default:
                abort();
              }
              ++i;
            }
          }
          auto dest_reg = REG_OBJ(); // ctx
          ASM_INVOKE_O(
              dest_reg,
              func_reg_no
          );
          return dest_reg;
        }
        break;
      }
      default:
        MVM_panic(MVM_exitcode_compunit, "Not implemented op: %s", PVIP_node_name(node->type));
        break;
      }
      printf("Should not reach here: %s\n", PVIP_node_name(node->type));
      abort();
    }
    int KijiCompiler::to_o(int reg_num) {
      assert(reg_num != UNKNOWN_REG);
      auto reg_type = Kiji_compiler_get_local_type(this, reg_num);
      if (reg_type == MVM_reg_obj) {
        return reg_num;
      }

      int dst_num = REG_OBJ();
      int boxtype_reg = REG_OBJ();
      switch (reg_type) {
      case MVM_reg_str:
        ASM_HLLBOXTYPE_S(boxtype_reg);
        ASM_BOX_S(dst_num, reg_num, boxtype_reg);
        return dst_num;
      case MVM_reg_int64:
        ASM_HLLBOXTYPE_I(boxtype_reg);
        ASM_BOX_I(dst_num, reg_num, boxtype_reg);
        return dst_num;
      case MVM_reg_num64:
        ASM_HLLBOXTYPE_N(boxtype_reg);
        ASM_BOX_N(dst_num, reg_num, boxtype_reg);
        return dst_num;
      default:
        MVM_panic(MVM_exitcode_compunit, "Not implemented, boxify %d", Kiji_compiler_get_local_type(this, reg_num));
        abort();
      }
    }
    int KijiCompiler::to_n(int reg_num) {
      assert(reg_num != UNKNOWN_REG);
      switch (Kiji_compiler_get_local_type(this, reg_num)) {
      case MVM_reg_str: {
        int dst_num = REG_NUM64();
        ASM_COERCE_SN(dst_num, reg_num);
        return dst_num;
      }
      case MVM_reg_num64: {
        return reg_num;
      }
      case MVM_reg_int64: {
        int dst_num = REG_NUM64();
        ASM_COERCE_IN(dst_num, reg_num);
        return dst_num;
      }
      case MVM_reg_obj: {
        int dst_num = REG_NUM64();
        ASM_SMRT_NUMIFY(dst_num, reg_num);
        return dst_num;
      }
      default:
        // TODO
        MVM_panic(MVM_exitcode_compunit, "Not implemented, numify %d", Kiji_compiler_get_local_type(this, reg_num));
        break;
      }
    }
    int KijiCompiler::to_i(int reg_num) {
      assert(reg_num != UNKNOWN_REG);
      switch (Kiji_compiler_get_local_type(this, reg_num)) {
      case MVM_reg_str: {
        int dst_num = REG_INT64();
        ASM_COERCE_SI(dst_num, reg_num);
        return dst_num;
      }
      case MVM_reg_num64: {
        int dst_num = REG_NUM64();
        ASM_COERCE_NI(dst_num, reg_num);
        return dst_num;
      }
      case MVM_reg_int64: {
        return reg_num;
      }
      case MVM_reg_obj: {
        int dst_num = REG_NUM64();
        int dst_int = REG_INT64();
        // TODO: I need smrt_intify?
        ASM_SMRT_NUMIFY(dst_num, reg_num);
        ASM_COERCE_NI(dst_int, dst_num);
        return dst_int;
      }
      default:
        // TODO
        MVM_panic(MVM_exitcode_compunit, "Not implemented, numify %d", Kiji_compiler_get_local_type(this, reg_num));
        break;
      }
    }
    int KijiCompiler::to_s(int reg_num) {
      assert(reg_num != UNKNOWN_REG);
      switch (Kiji_compiler_get_local_type(this, reg_num)) {
      case MVM_reg_str:
        // nop
        return reg_num;
      case MVM_reg_num64: {
        int dst_num = REG_STR();
        ASM_COERCE_NS(dst_num, reg_num);
        return dst_num;
      }
      case MVM_reg_int64: {
        int dst_num = REG_STR();
        ASM_COERCE_IS(dst_num, reg_num);
        return dst_num;
      }
      case MVM_reg_obj: {
        int dst_num = REG_STR();
        ASM_SMRT_STRIFY(dst_num, reg_num);
        return dst_num;
      }
      default:
        // TODO
        MVM_panic(MVM_exitcode_compunit, "Not implemented, to_s %d", Kiji_compiler_get_local_type(this, reg_num));
        break;
      }
    }
    int KijiCompiler::str_binop(const PVIPNode* node, uint16_t op) {
        assert(node->children.size == 2);

        int reg_num1 = to_s(do_compile(node->children.nodes[0]));
        int reg_num2 = to_s(do_compile(node->children.nodes[1]));
        int reg_num_dst = REG_INT64();
        ASM_OP_U16_U16_U16(MVM_OP_BANK_string, op, reg_num_dst, reg_num1, reg_num2);
        return reg_num_dst;
    }
    int KijiCompiler::binary_binop(const PVIPNode* node, uint16_t op_i) {
        assert(node->children.size == 2);

        int reg_num1 = to_i(do_compile(node->children.nodes[0]));
        int reg_num2 = to_i(do_compile(node->children.nodes[1]));
        auto dst_reg = REG_INT64();
        ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_i, dst_reg, reg_num1, reg_num2);
        return dst_reg;
    }
    int KijiCompiler::numeric_inplace(const PVIPNode* node, uint16_t op_i, uint16_t op_n) {
        assert(node->children.size == 2);
        assert(node->children.nodes[0]->type == PVIP_NODE_VARIABLE);

        auto lhs = get_variable(node->children.nodes[0]->pv);
        auto rhs = do_compile(node->children.nodes[1]);
        auto tmp = REG_NUM64();
        ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_n, tmp, to_n(lhs), to_n(rhs));
        set_variable(node->children.nodes[0]->pv, to_o(tmp));
        return tmp;
    }
    int KijiCompiler::binary_inplace(const PVIPNode* node, uint16_t op) {
        assert(node->children.size == 2);
        assert(node->children.nodes[0]->type == PVIP_NODE_VARIABLE);

        auto lhs = get_variable(node->children.nodes[0]->pv);
        auto rhs = do_compile(node->children.nodes[1]);
        auto tmp = REG_INT64();
        ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op, tmp, to_i(lhs), to_i(rhs));
        set_variable(node->children.nodes[0]->pv, to_o(tmp));
        return tmp;
    }
    int KijiCompiler::str_inplace(const PVIPNode* node, uint16_t op, uint16_t rhs_type) {
        assert(node->children.size == 2);
        assert(node->children.nodes[0]->type == PVIP_NODE_VARIABLE);

        auto lhs = get_variable(node->children.nodes[0]->pv);
        auto rhs = do_compile(node->children.nodes[1]);
        auto tmp = REG_STR();
        ASM_OP_U16_U16_U16(MVM_OP_BANK_string, op, tmp, to_s(lhs), rhs_type == MVM_reg_int64 ? to_i(rhs) : to_s(rhs));
        set_variable(node->children.nodes[0]->pv, to_o(tmp));
        return tmp;
    }
    int KijiCompiler::numeric_binop(const PVIPNode* node, uint16_t op_i, uint16_t op_n) {
        assert(node->children.size == 2);

        int reg_num1 = do_compile(node->children.nodes[0]);
        if (Kiji_compiler_get_local_type(this, reg_num1) == MVM_reg_int64) {
          int reg_num_dst = REG_INT64();
          int reg_num2 = this->to_i(do_compile(node->children.nodes[1]));
          assert(Kiji_compiler_get_local_type(this, reg_num1) == MVM_reg_int64);
          assert(Kiji_compiler_get_local_type(this, reg_num2) == MVM_reg_int64);
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_i, reg_num_dst, reg_num1, reg_num2);
          return reg_num_dst;
        } else if (Kiji_compiler_get_local_type(this, reg_num1) == MVM_reg_num64) {
          int reg_num_dst = REG_NUM64();
          int reg_num2 = this->to_n(do_compile(node->children.nodes[1]));
          assert(Kiji_compiler_get_local_type(this, reg_num2) == MVM_reg_num64);
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_n, reg_num_dst, reg_num1, reg_num2);
          return reg_num_dst;
        } else if (Kiji_compiler_get_local_type(this, reg_num1) == MVM_reg_obj) {
          // TODO should I use intify instead if the object is int?
          int reg_num_dst = REG_NUM64();

          int dst_num = REG_NUM64();
          ASM_OP_U16_U16(MVM_OP_BANK_primitives, MVM_OP_smrt_numify, dst_num, reg_num1);

          int reg_num2 = this->to_n(do_compile(node->children.nodes[1]));
          assert(Kiji_compiler_get_local_type(this, reg_num2) == MVM_reg_num64);
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_n, reg_num_dst, dst_num, reg_num2);
          return reg_num_dst;
        } else if (Kiji_compiler_get_local_type(this, reg_num1) == MVM_reg_str) {
          int dst_num = REG_NUM64();
          ASM_COERCE_SN(dst_num, reg_num1);

          int reg_num_dst = REG_NUM64();
          int reg_num2 = this->to_n(do_compile(node->children.nodes[1]));
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_n, reg_num_dst, dst_num, reg_num2);

          return reg_num_dst;
        } else {
          abort();
        }
    }

    uint16_t KijiCompiler::unless_op(uint16_t cond_reg) {
      switch (Kiji_compiler_get_local_type(this, cond_reg)) {
      case MVM_reg_int64:
        return MVM_OP_unless_i;
      case MVM_reg_num64:
        return MVM_OP_unless_n;
      case MVM_reg_str:
        return MVM_OP_unless_s;
      case MVM_reg_obj:
        return MVM_OP_unless_o;
      default:
        abort();
      }
    }
    void KijiCompiler::compile_statements(const PVIPNode*node, int dst_reg) {
      int reg = UNKNOWN_REG;
      if (node->type == PVIP_NODE_STATEMENTS || node->type == PVIP_NODE_ELSE) {
        for (int i=0, l=node->children.size; i<l; i++) {
          reg = do_compile(node->children.nodes[i]);
        }
      } else {
        reg = do_compile(node);
      }
      if (reg == UNKNOWN_REG) {
        ASM_NULL(dst_reg);
      } else {
        ASM_SET(dst_reg, to_o(reg));
      }
    }

    // Compile chained comparisions like `1 < $n < 3`.
    // TODO: optimize simple case like `1 < $n`
    uint16_t KijiCompiler::compile_chained_comparisions(const PVIPNode* node) {
      auto lhs = do_compile(node->children.nodes[0]);
      auto dst_reg = REG_INT64();
      auto label_end = label_unsolved();
      auto label_false = label_unsolved();
      for (int i=1; i<node->children.size; i++) {
        PVIPNode *iter = node->children.nodes[i];
        auto rhs = do_compile(iter->children.nodes[0]);
        // result will store to lhs.
        uint16_t ret = do_compare(iter->type, lhs, rhs);
        unless_any(ret, label_false);
        lhs = rhs;
      }
      ASM_CONST_I64(dst_reg, 1);
      goto_(label_end);
    label_false.put();
      ASM_CONST_I64(dst_reg, 0);
      // goto_(label_end());
    label_end.put();
      return dst_reg;
    }
    int KijiCompiler::num_cmp_binop(uint16_t lhs, uint16_t rhs, uint16_t op_i, uint16_t op_n) {
        int reg_num_dst = REG_INT64();
        if (Kiji_compiler_get_local_type(this, lhs) == MVM_reg_int64) {
          assert(Kiji_compiler_get_local_type(this, lhs) == MVM_reg_int64);
          // assert(Kiji_compiler_get_local_type(this, rhs) == MVM_reg_int64);
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_i, reg_num_dst, lhs, to_i(rhs));
          return reg_num_dst;
        } else if (Kiji_compiler_get_local_type(this, lhs) == MVM_reg_num64) {
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_n, reg_num_dst, lhs, to_n(rhs));
          return reg_num_dst;
        } else if (Kiji_compiler_get_local_type(this, lhs) == MVM_reg_obj) {
          // TODO should I use intify instead if the object is int?
          ASM_OP_U16_U16_U16(MVM_OP_BANK_primitives, op_n, reg_num_dst, to_n(lhs), to_n(rhs));
          return reg_num_dst;
        } else {
          // NOT IMPLEMENTED
          abort();
        }
    }

    int KijiCompiler::str_cmp_binop(uint16_t lhs, uint16_t rhs, uint16_t op) {
        int reg_num_dst = REG_INT64();
        ASM_OP_U16_U16_U16(MVM_OP_BANK_string, op, reg_num_dst, to_s(lhs), to_s(rhs));
        return reg_num_dst;
    }
    uint16_t KijiCompiler::do_compare(PVIP_node_type_t type, uint16_t lhs, uint16_t rhs) {
      switch (type) {
      case PVIP_NODE_STREQ:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_eq_s);
      case PVIP_NODE_STRNE:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_ne_s);
      case PVIP_NODE_STRGT:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_gt_s);
      case PVIP_NODE_STRGE:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_ge_s);
      case PVIP_NODE_STRLT:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_lt_s);
      case PVIP_NODE_STRLE:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_le_s);
      case PVIP_NODE_EQ:
        return this->num_cmp_binop(lhs, rhs, MVM_OP_eq_i, MVM_OP_eq_n);
      case PVIP_NODE_NE:
        return this->num_cmp_binop(lhs, rhs, MVM_OP_ne_i, MVM_OP_ne_n);
      case PVIP_NODE_LT:
        return this->num_cmp_binop(lhs, rhs, MVM_OP_lt_i, MVM_OP_lt_n);
      case PVIP_NODE_LE:
        return this->num_cmp_binop(lhs, rhs, MVM_OP_le_i, MVM_OP_le_n);
      case PVIP_NODE_GT:
        return this->num_cmp_binop(lhs, rhs, MVM_OP_gt_i, MVM_OP_gt_n);
      case PVIP_NODE_GE:
        return this->num_cmp_binop(lhs, rhs, MVM_OP_ge_i, MVM_OP_ge_n);
      case PVIP_NODE_EQV:
        return this->str_cmp_binop(lhs, rhs, MVM_OP_eq_s);
      default:
        abort();
      }
    }

    KijiCompiler::KijiCompiler(MVMCompUnit * cu, MVMThreadContext * tc): cu_(cu), frame_no_(0), tc_(tc) {
      initialize();

      current_class_how_ = NULL;

      auto handle = MVM_string_ascii_decode_nt(tc, tc->instance->VMString, (char*)"__SARU_CLASSES__");
      assert(tc);
      sc_classes_ = (MVMSerializationContext*)MVM_sc_create(tc_, handle);

      num_sc_classes_ = 0;
    }

    void KijiCompiler::initialize() {
      // init compunit.
      int apr_return_status;
      apr_pool_t  *pool        = NULL;
      /* Ensure the file exists, and get its size. */
      if ((apr_return_status = apr_pool_create(&pool, NULL)) != APR_SUCCESS) {
        MVM_panic(MVM_exitcode_compunit, "Could not allocate APR memory pool: errorcode %d", apr_return_status);
      }
      cu_->pool       = pool;
      assert(tc_);
      Kiji_compiler_push_frame(this, std::string("frame_name_0"));
    }
    void KijiCompiler::finalize(MVMInstance* vm) {
      MVMThreadContext *tc = tc_; // remove me
      MVMInstance *vm_ = vm; // remove me

      // finalize frame
      for (int i=0; i<cu_->num_frames; i++) {
        MVMStaticFrame *frame = cu_->frames[i];
        // XXX Should I need to time sizeof(MVMRegister)??
        frame->env_size = frame->num_lexicals * sizeof(MVMRegister);
        Newxz(frame->static_env, frame->env_size, MVMRegister);

        char buf[1023+1];
        int len = snprintf(buf, 1023, "frame_cuuid_%d", i);
        frame->cuuid = MVM_string_utf8_decode(tc_, tc_->instance->VMString, buf, len);
      }

      cu_->main_frame = cu_->frames[0];
      assert(cu_->main_frame->cuuid);

      // Creates code objects to go with each of the static frames.
      // ref create_code_objects in src/core/bytecode.c
      Newxz(cu_->coderefs, cu_->num_frames, MVMObject*);

      MVMObject* code_type = tc->instance->boot_types->BOOTCode;

      for (int i = 0; i < cu_->num_frames; i++) {
        cu_->coderefs[i] = REPR(code_type)->allocate(tc, STABLE(code_type));
        ((MVMCode *)cu_->coderefs[i])->body.sf = cu_->frames[i];
      }
    }

    void KijiCompiler::compile(PVIPNode*node, MVMInstance* vm) {
      ASM_CHECKARITY(0, -1);

      /*
      int code = REG_OBJ();
      int dest_reg = REG_OBJ();
      ASM_WVAL(code, 0, 1);
      MVMCallsite* callsite = new MVMCallsite;
      memset(callsite, 0, sizeof(MVMCallsite));
      callsite->arg_count = 0;
      callsite->num_pos = 0;
      callsite->arg_flags = new MVMCallsiteEntry[0];
      // callsite->arg_flags[0] = MVM_CALLSITE_ARG_OBJ;;

      auto callsite_no = interp_.push_callsite(callsite);
      ASM_PREPARGS(callsite_no);
      ASM_INVOKE_O( dest_reg, code);
      */

      // bootstrap $?PACKAGE
      // TODO I should wrap it to any object. And set WHO.
      // $?PACKAGE.WHO<bar> should work.
      {
        auto lex = push_lexical("$?PACKAGE", MVM_reg_obj);
        auto package = REG_OBJ();
        auto hash_type = REG_OBJ();
        ASM_HLLHASH(hash_type);
        ASM_CREATE(package, hash_type);
        ASM_BINDLEX(lex, 0, package);
      }

      do_compile(node);

      // bootarray
      /*
      int ary = interp_.push_local_type(MVM_reg_obj);
      ASM_BOOTARRAY(ary);
      ASM_CREATE(ary, ary);
      ASM_PREPARGS(1);
      ASM_INVOKE_O(ary, ary);

      ASM_BOOTSTRARRAY(ary);
      ASM_CREATE(ary, ary);
      ASM_PREPARGS(1);
      ASM_INVOKE_O(ary, ary);
      */

      // final op must be return.
      int reg = REG_OBJ();
      ASM_NULL(reg);
      ASM_RETURN_O(reg);

      // setup hllconfig
      MVMThreadContext * tc = tc_;
      {
        MVMString *hll_name = MVM_string_ascii_decode_nt(tc, tc->instance->VMString, (char*)"kiji");
        CU->hll_config = MVM_hll_get_config_for(tc, hll_name);

        MVMObject *config = REPR(tc->instance->boot_types->BOOTHash)->allocate(tc, STABLE(tc->instance->boot_types->BOOTHash));
        MVM_hll_set_config(tc, hll_name, config);
      }

      // hacking hll
      Kiji_bootstrap_Array(CU, tc);
      Kiji_bootstrap_Str(CU,   tc);
      Kiji_bootstrap_Hash(CU,  tc);
      Kiji_bootstrap_File(CU,  tc);
      Kiji_bootstrap_Int(CU,   tc);

      // finalize callsite
      CU->max_callsite_size = 0;
      for (int i=0; i<CU->num_callsites; i++) {
        auto callsite = CU->callsites[i];
        CU->max_callsite_size = std::max(CU->max_callsite_size, callsite->arg_count);
      }

      // Initialize @*ARGS
      MVMObject *clargs = MVM_repr_alloc_init(tc, tc->instance->boot_types->BOOTArray);
      MVM_gc_root_add_permanent(tc, (MVMCollectable **)&clargs);
      auto handle = MVM_string_ascii_decode_nt(tc, tc->instance->VMString, (char*)"__SARU_CORE__");
      auto sc = (MVMSerializationContext *)MVM_sc_create(tc, handle);
      MVMROOT(tc, sc, {
        MVMROOT(tc, clargs, {
          MVMint64 count;
          for (count = 0; count < vm->num_clargs; count++) {
            MVMString *string = MVM_string_utf8_decode(tc,
              tc->instance->VMString,
              vm->raw_clargs[count], strlen(vm->raw_clargs[count])
            );
            MVMObject*type = CU->hll_config->str_box_type;
            MVMObject *box = REPR(type)->allocate(tc, STABLE(type));
            MVMROOT(tc, box, {
                if (REPR(box)->initialize)
                    REPR(box)->initialize(tc, STABLE(box), box, OBJECT_BODY(box));
                REPR(box)->box_funcs->set_str(tc, STABLE(box), box,
                    OBJECT_BODY(box), string);
            });
            MVM_repr_push_o(tc, clargs, box);
          }
          MVM_sc_set_object(tc, sc, 0, clargs);
        });
      });

      CU->num_scs = 2;
      CU->scs = (MVMSerializationContext**)malloc(sizeof(MVMSerializationContext*)*2);
      CU->scs[0] = sc;
      CU->scs[1] = sc_classes_;
      CU->scs_to_resolve = (MVMString**)malloc(sizeof(MVMString*)*2);
      CU->scs_to_resolve[0] = NULL;
      CU->scs_to_resolve[1] = NULL;
    }
