// vim:ts=2:sw=2:tw=0:

#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#ifdef __unix__
#include <unistd.h>
#endif
#include <moarvm.h>
#include "commander.h"
#include "compiler.h"
#include "asm.h"

typedef struct _CmdLineState {
    int dump_ast;
    int dump_bytecode;
    const char* eval;
} CmdLineState;

static void dump_bytecode(command_t *self) {
    ((CmdLineState*)self->data)->dump_bytecode = 1;
}

static void dump_ast(command_t *self) {
    ((CmdLineState*)self->data)->dump_ast = 1;
}

static void eval(command_t *self) {
    ((CmdLineState*)self->data)->eval = self->arg;
}

static void toplevel_initial_invoke(MVMThreadContext *tc, void *data) {
  /* Dummy, 0-arg callsite. */
  static MVMCallsite no_arg_callsite;
  no_arg_callsite.arg_flags = NULL;
  no_arg_callsite.arg_count = 0;
  no_arg_callsite.num_pos   = 0;

  /* Create initial frame, which sets up all of the interpreter state also. */
  MVM_frame_invoke(tc, (MVMStaticFrame *)data, &no_arg_callsite, NULL, NULL, NULL);
}

static void run_repl() {
  // TODO disabled for now(pvip)
  /*
  std::string src;
  while (!std::cin.eof()) {
    std::cout << "> ";
    std::cin >> src;

    {
      std::unique_ptr<std::istringstream> iss(new std::istringstream(src));

      kiji::Node root;

      if (!kiji::parse(&(*iss), root)) {
        continue;
      }

      kiji::Interpreter interp;
      kiji::CompUnit cu(interp.main_thread());
      kiji::Compiler compiler(cu);
      compiler.compile(root);
      interp.run(cu);
    }
    std::cout << std::endl;
  }
  */
}

int main(int argc, char** argv) {
  CmdLineState*state;
  Newxz(state, 1, CmdLineState);

  command_t cmd;
  cmd.data = state;
  command_init(&cmd, argv[0], "0.0.1");
  command_option(&cmd, "-q", "--dump-bytecode", "dump bytecode", dump_bytecode);
  command_option(&cmd, "-p", "--dump-ast", "dump Abstract Syntax Tree", dump_ast);
  command_option(&cmd, "-e", "--eval [code]", "eval code", eval);
  command_parse(&cmd, argc, argv);

  int processed_args = 0;

  PVIPNode *root_node;
  PVIPString * error;
  if (state->eval) {
    root_node = PVIP_parse_string(state->eval, strlen(state->eval), 0, &error);
  } else if (cmd.argc==0) {
#ifdef __unix__
    if (isatty(fileno(stdin))) {
      run_repl();
      exit(0);
    }
#endif

    root_node = PVIP_parse_fp(stdin, 0, &error);
  } else {
    FILE *fp = fopen(cmd.argv[0], "rb");
    if (!fp) {
      printf("Cannot open file %s for reading: %s", cmd.argv[0], strerror(errno));
      exit(1);
    }
    root_node = PVIP_parse_fp(fp, 0, &error);
  }

  if (!root_node) {
    PVIP_string_say(error);
    exit(1);
  }

  if (state->dump_ast) {
    PVIP_node_dump_sexp(root_node);
    return 0;
  }

  MVMInstance* vm = MVM_vm_create_instance();

  // stash the rest of the raw command line args in the instance
  vm->num_clargs = cmd.argc-1;
  vm->raw_clargs = cmd.argv+1;

  MVMCompUnit cu;
  memset(&cu, 0, sizeof(MVMCompUnit));

  int apr_return_status;
  apr_pool_t  *pool        = NULL;
  /* Ensure the file exists, and get its size. */
  if ((apr_return_status = apr_pool_create(&pool, NULL)) != APR_SUCCESS) {
    MVM_panic(MVM_exitcode_compunit, "Could not allocate APR memory pool: errorcode %d", apr_return_status);
  }
  cu.pool       = pool;

  KijiCompiler compiler;
  Kiji_compiler_init(&compiler, &cu, vm->main_thread, vm);
  Kiji_compiler_compile(&compiler, root_node, vm);
  Kiji_compiler_finalize(&compiler, vm);
#ifdef DEBUG_ASM
  Kiji_asm_dump_compunit(&cu);
#endif

  if (state->dump_bytecode) {
    // dump it
    char *dump = MVM_bytecode_dump(vm->main_thread, &cu);

    printf("%s", dump);
    free(dump);
  } else {
    MVMThreadContext *tc = vm->main_thread;
    MVMStaticFrame *start_frame = cu.main_frame ? cu.main_frame : cu.frames[0];
    MVM_interp_run(tc, &toplevel_initial_invoke, start_frame);
  }

  MVM_vm_destroy_instance(vm);

  return 0;
}
