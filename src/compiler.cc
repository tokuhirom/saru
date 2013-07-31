/* vim:ts=2:sw=2:tw=0:
 */

extern "C" {
#include "moarvm.h"
}
#include "compiler.h"
#include "builtin.h"
#include "asm.h"
#include "compiler/loop.h"
#include "gen.assembler.h"
#include "compiler/gen.nd.h"

#include <string>

// This reg returns register number contains true value.
int Kiji_compiler_do_compile(KijiCompiler *self, const PVIPNode*node) {
  int ret = Kiji_compiler_compile_nodes(self, node);
  if (ret != -18185963) {
    return ret;
  }

  // printf("node: %s\n", node.type_name());
  switch (node->type) {
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
    LABEL(label_end);
    LABEL(label_else);
    MVMuint16 dst_reg = REG_OBJ();

      int cond_reg = Kiji_compiler_do_compile(self, node->children.nodes[0]);
      Kiji_compiler_unless_any(self, cond_reg, &label_else);

      int if_reg = Kiji_compiler_do_compile(self, node->children.nodes[1]);
      ASM_SET(dst_reg, Kiji_compiler_to_o(self, if_reg));
      Kiji_compiler_goto(self, &label_end);

    LABEL_PUT(label_else);
      int else_reg = Kiji_compiler_do_compile(self, node->children.nodes[2]);
      ASM_SET(dst_reg, Kiji_compiler_to_o(self, else_reg));

    LABEL_PUT(label_end);

    return dst_reg;
  }
  case PVIP_NODE_NOT: {
    MVMuint16 src_reg = Kiji_compiler_to_i(self, Kiji_compiler_do_compile(self, node->children.nodes[0]));
    MVMuint16 dst_reg = REG_INT64();
    ASM_NOT_I(dst_reg, src_reg);
    return dst_reg;
  }
  case PVIP_NODE_NOP:
    return -1;
  case PVIP_NODE_ATKEY: {
    assert(node->children.size == 2);
    MVMuint16 dst       = REG_OBJ();
    MVMuint16 container = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[0]));
    MVMuint16 key       = Kiji_compiler_to_s(self, Kiji_compiler_do_compile(self, node->children.nodes[1]));
    ASM_ATKEY_O(dst, container, key);
    return dst;
  }
  case PVIP_NODE_HASH: {
    MVMuint16 hashtype = REG_OBJ();
    MVMuint16 hash     = REG_OBJ();
    ASM_HLLHASH(hashtype);
    ASM_CREATE(hash, hashtype);
    for (int i=0; i<node->children.size; i++) {
      PVIPNode* pair = node->children.nodes[i];
      assert(pair->type == PVIP_NODE_PAIR);
      MVMuint16 k = Kiji_compiler_to_s(self, Kiji_compiler_do_compile(self, pair->children.nodes[0]));
      MVMuint16 v = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, pair->children.nodes[1]));
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
    LABEL(label_both_false);
    LABEL(label_a1_true);
    LABEL(label_both_true);
    LABEL(label_end);

    MVMuint16 dst_reg = REG_OBJ();

      MVMuint16 arg1 = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[0]));
      MVMuint16 arg2 = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[1]));
      Kiji_compiler_if_any(self, arg1, &label_a1_true);
      Kiji_compiler_unless_any(self, arg2, &label_both_false);
      ASM_SET(dst_reg, arg2);
      Kiji_compiler_goto(self, &label_end);
    LABEL_PUT(label_both_false);
      ASM_SET(dst_reg, arg1);
      Kiji_compiler_goto(self, &label_end);
    LABEL_PUT(label_a1_true); // a1:true, a2:unknown
      Kiji_compiler_if_any(self, arg2, &label_both_true);
      ASM_SET(dst_reg, arg1);
      Kiji_compiler_goto(self, &label_end);
    LABEL_PUT(label_both_true);
      ASM_NULL(dst_reg);
      Kiji_compiler_goto(self, &label_end);
    LABEL_PUT(label_end);
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
    LABEL(label_end);
    LABEL(label_a1);
    MVMuint16 dst_reg = REG_OBJ();
      MVMuint16 arg1 = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[0]));
      Kiji_compiler_if_any(self, arg1, &label_a1);
      MVMuint16 arg2 = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[1]));
      ASM_SET(dst_reg, arg2);
      Kiji_compiler_goto(self, &label_end);
    LABEL_PUT(label_a1);
      ASM_SET(dst_reg, arg1);
    LABEL_PUT(label_end);
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
    LABEL(label_end);
    LABEL(label_a1);
    MVMuint16 dst_reg = REG_OBJ();
      MVMuint16 arg1 = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[0]));
      Kiji_compiler_unless_any(self, arg1, &label_a1);
      MVMuint16 arg2 = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[1]));
      ASM_SET(dst_reg, arg2);
      Kiji_compiler_goto(self, &label_end);
    LABEL_PUT(label_a1);
      ASM_SET(dst_reg, arg1);
    LABEL_PUT(label_end);
    return dst_reg;
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
          uint16_t reg_num = Kiji_compiler_to_s(self, Kiji_compiler_do_compile(self, a));
          if (i==args->children.size-1) {
            ASM_SAY(reg_num);
          } else {
            ASM_PRINT(reg_num);
          }
        }
        return Kiji_compiler_const_true(self);
      } else if (std::string(ident->pv->buf, ident->pv->len) == "print") {
        for (int i=0; i<args->children.size; i++) {
          PVIPNode* a = args->children.nodes[i];
          uint16_t reg_num = Kiji_compiler_to_s(self, Kiji_compiler_do_compile(self, a));
          ASM_PRINT(reg_num);
        }
        return Kiji_compiler_const_true(self);
      } else if (std::string(ident->pv->buf, ident->pv->len) == "open") {
        // TODO support arguments
        assert(args->children.size == 1);
        int fname_s = Kiji_compiler_do_compile(self, args->children.nodes[0]);
        MVMuint16 dst_reg_o = REG_OBJ();
        // TODO support latin1, etc.
        int mode = Kiji_compiler_push_string(self, newMVMString_nolen("r"));
        MVMuint16 mode_s = REG_STR();
        ASM_CONST_S(mode_s, mode);
        ASM_OPEN_FH(dst_reg_o, fname_s, mode_s);
        return dst_reg_o;
      } else if (std::string(ident->pv->buf, ident->pv->len) == "slurp") {
        assert(args->children.size <= 2);
        assert(args->children.size != 2 && "Encoding option is not supported yet");
        MVMuint16 fname_s = Kiji_compiler_do_compile(self, args->children.nodes[0]);
        MVMuint16 dst_reg_s = REG_STR();
        MVMuint16 encoding_s = REG_STR();
        ASM_CONST_S(encoding_s, Kiji_compiler_push_string(self, newMVMString_nolen("utf8"))); // TODO support latin1, etc.
        ASM_SLURP(dst_reg_s, fname_s, encoding_s);
        return dst_reg_s;
      }
    }

    {
      uint16_t func_reg_no;
      if (node->children.nodes[0]->type == PVIP_NODE_IDENT) {
        func_reg_no = REG_OBJ();
        int lex_no, outer;
        if (!Kiji_compiler_find_lexical_by_name(self, newMVMStringFromSTDSTRING(std::string("&") + std::string(ident->pv->buf, ident->pv->len)), &lex_no, &outer)) {
          MVM_panic(MVM_exitcode_compunit, "Unknown lexical variable in find_lexical_by_name: %s\n", (std::string("&") + std::string(ident->pv->buf, ident->pv->len)).c_str());
        }
        ASM_GETLEX(
          func_reg_no,
          lex_no,
          outer // outer frame
        );
      } else {
        func_reg_no = Kiji_compiler_to_o(self, Kiji_compiler_do_compile(self, node->children.nodes[0]));
      }

      {
        MVMCallsite* callsite;
        Newxz(callsite, 1, MVMCallsite);
        // TODO support named params
        callsite->arg_count = args->children.size;
        callsite->num_pos   = args->children.size;
        Newxz(callsite->arg_flags, args->children.size, MVMCallsiteEntry);

        uint16_t *arg_regs  = NULL;
        size_t num_arg_regs = 0;

        for (int i=0; i<args->children.size; i++) {
          PVIPNode *a = args->children.nodes[i];
          int reg = Kiji_compiler_do_compile(self, a);
          if (reg<0) {
            MVM_panic(MVM_exitcode_compunit, "Compilation error. You should not pass void function as an argument: %s", PVIP_node_name(a->type));
          }

          switch (Kiji_compiler_get_local_type(self, reg)) {
          case MVM_reg_int64:
            callsite->arg_flags[i] = MVM_CALLSITE_ARG_INT;
            break;
          case MVM_reg_num64:
            callsite->arg_flags[i] = MVM_CALLSITE_ARG_NUM;
            break;
          case MVM_reg_str:
            callsite->arg_flags[i] = MVM_CALLSITE_ARG_STR;
            break;
          case MVM_reg_obj:
            callsite->arg_flags[i] = MVM_CALLSITE_ARG_OBJ;
            break;
          default:
            abort();
          }

          num_arg_regs++;
          Renew(arg_regs, num_arg_regs, uint16_t);
          arg_regs[num_arg_regs-1] = reg;
        }

        int callsite_no = Kiji_compiler_push_callsite(self, callsite);
        ASM_PREPARGS(callsite_no);

        int i;
        for (i=0; i<num_arg_regs; ++i) {
          uint16_t reg = arg_regs[i];
          switch (Kiji_compiler_get_local_type(self, reg)) {
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
        }
      }
      MVMuint16 dest_reg = REG_OBJ(); // ctx
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


