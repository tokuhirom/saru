// vim:ts=2:sw=2:tw=0:
#include <stdio.h>
#include <assert.h>
#include <fstream>
#ifdef __unix__
#include <unistd.h>
#endif
#include "node.h"
#include "gen.saru.y.cc"
extern "C" {
#include "moarvm.h"
}
#include "compiler.h"

bool parse() {
  bool retval = true;
  GREG g;
  yyinit(&g);
  if (!yyparse(&g)) {
    fprintf(stderr, "** Syntax error at line %d\n", line_number);
    if (g.text[0]) {
      fprintf(stderr, "** near %s\n", g.text);
    }
    if (g.pos < g.limit || !feof(stdin)) {
      g.buf[g.limit]= '\0';
      fprintf(stderr, " before text \"");
      while (g.pos < g.limit) {
        if ('\n' == g.buf[g.pos] || '\r' == g.buf[g.pos]) break;
        fputc(g.buf[g.pos++], stderr);
      }
      if (g.pos == g.limit) {
        int c;
        while (EOF != (c= fgetc(stdin)) && '\n' != c && '\r' != c)
        fputc(c, stderr);
      }
      fputc('\"', stderr);
    }
    fprintf(stderr, "\n");
    retval = false;
  }
  yydeinit(&g);
  return retval;
}

void run_repl() {
  std::string src;
  while (!std::cin.eof()) {
    std::cout << "> ";
    std::cin >> src;

    {
      std::unique_ptr<std::istringstream> iss(new std::istringstream(src));
      global_input_stream = &(*iss);

      if (!parse()) {
        continue;
      }

      saru::Interpreter interp;
      interp.initialize();
      saru::Compiler compiler(interp);
      compiler.compile(node_global);
      interp.run();
    }
    std::cout << std::endl;
  }
}

int main(int argc, char** argv) {
  line_number=0;

  // This include apr_initialize().
  // You need to initialize before `apr_pool_create()`
  saru::Interpreter interp;
  interp.initialize();

  apr_status_t rv;
  apr_pool_t *mp;
  static const apr_getopt_option_t opt_option[] = {
      /* values must be greater than 255 so it doesn't have a single-char form
          Otherwise, use a character such as 'h' */
      { "dump-bytecode", 256, 0, "dump bytecode" },
      { "help", 257, 0, "show help" },
      { "dump-ast", 258, 0, "dump ast" },
      { NULL, 'e', 1, "eval" },
      { NULL, 0, 0, NULL }
  };
  apr_getopt_t *opt;
  int optch;
  const char *optarg;
  const char *eval = NULL;
  bool dump_ast = false;
  bool dump_bytecode = false;
  const char *helptext = "\
  MoarVM usage: moarvm [options] bytecode.moarvm [program args]           \n\
    --help, display this message                                          \n\
    --dump, dump the bytecode to stdout instead of executing              \n";
  int processed_args = 0;

  apr_pool_create(&mp, NULL);
  apr_getopt_init(&opt, mp, argc, argv);
  while ((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
      switch (optch) {
      case 'e':
        eval = strdup(optarg);
        break;
      case 256:
        dump_bytecode = true;
        break;
      case 257:
        printf("%s", helptext);
        exit(1);
      case 258:
        dump_ast = true;
        break;
      }
  }

  processed_args = opt->ind;
  if (eval) {
    global_input_stream = new std::istringstream(eval);
  } else if (processed_args == argc) {
#ifdef __unix__
    if (isatty(fileno(stdin))) {
      run_repl();
      exit(0);
    }
#endif

    global_input_stream = &std::cin;
  } else {
    global_input_stream = new std::ifstream((char *)opt->argv[processed_args++]);
    if (!*global_input_stream) {
      std::cerr << "Cannot open file: " << opt->argv[processed_args-1] << std::endl;
      exit(1);
    }
  }
  // stash the rest of the raw command line args in the instance
  /*
  instance->num_clargs = argc - processed_args;
  instance->raw_clargs = (char **)(opt->argv + processed_args);
  */

  if (!parse()) {
    exit(1);
  }

  if (!global_input_stream->eof()) {
    printf("Syntax error! Around:\n");
    for (int i=0; !global_input_stream->eof() && i<24; i++) {
      char ch = global_input_stream->get();
      if (ch != EOF) {
        printf("%c", ch);
      }
    }
    printf("\n");
    exit(1);
  }

  if (dump_ast) {
    node_global.dump_json();
    return 0;
  }

  saru::Compiler compiler(interp);
  compiler.compile(node_global);
  if (dump_bytecode) {
    interp.dump();
  } else {
    interp.run();
  }

  return 0;
}
