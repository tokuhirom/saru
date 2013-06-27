%{

#include "node.h"
#include <iostream>

#define YYSTYPE kiji::Node

#define YY_XTYPE kiji::ParserContext

namespace kiji {
  struct ParserContext {
    int line_number;
    kiji::Node *root;
    std::istream *input_stream;
  };
};

#define YY_INPUT(buf, result, max_size, D)		\
  {							\
    int yyc= D.input_stream->get();					\
    result= (EOF == yyc) ? 0 : (*(buf)= yyc, 1);	\
    yyprintf((stderr, "<%c>", yyc));			\
  }

%}

comp_init = e:statementlist end-of-file {
    $$ = (*(G->data.root) = e);
}

statementlist =
    (
        s1:statement {
            $$.set(kiji::NODE_STATEMENTS, s1);
            s1 = $$;
        }
        (
            - s2:statement {
                s1.push_child(s2);
                $$ = s1;
            }
        )* eat_terminator?
    )

statement =
        - (
              use_stmt
            | e:postfix_if_stmt eat_terminator { $$ = e; }
            | e:postfix_unless_stmt eat_terminator { $$ = e; }
            | e:postfix_for_stmt eat_terminator { $$ = e; }
            | if_stmt
            | for_stmt
            | while_stmt
            | unless_stmt
            | module_stmt
            | class_stmt
            | method_stmt
            | die_stmt
            | last_stmt
            | funcdef - ';'*
            | bl:block { $$.set(kiji::NODE_BLOCK, bl); }
            | b:normal_stmt - eat_terminator { $$ = b; }
            | e:funcall_stmt eat_terminator { $$=e; }
          )

last_stmt = 'last' { $$.set_children(kiji::NODE_LAST); }

funcall_stmt =
    i:ident ' '+ a:args {
        // funcall without parens.
        $$.set(kiji::NODE_FUNCALL, i, a);
    }

class_stmt = 'class' ws i:ident - b:block { $$.set(kiji::NODE_CLASS, i, b); }

method_stmt = 'method' ws i:ident p:paren_args - b:block { $$.set(kiji::NODE_METHOD, i, p, b); }

normal_stmt = return_stmt | bind_stmt

return_stmt = 'return' ws e:expr { $$.set(kiji::NODE_RETURN, e); }

module_stmt = 'module' ws pkg:pkg_name eat_terminator { $$.set(kiji::NODE_MODULE, pkg); }

use_stmt = 'use ' pkg:pkg_name eat_terminator { $$.set(kiji::NODE_USE, pkg); }

pkg_name = < [a-zA-Z] [a-zA-Z0-9]* ( '::' [a-zA-Z0-9]+ )* > {
    $$.set_ident(yytext, yyleng);
}

die_stmt = 'die' ws e:expr eat_terminator { $$.set(kiji::NODE_DIE, e); }

while_stmt = 'while' ws+ cond:expr - '{' - body:statementlist - '}' {
            $$.set(kiji::NODE_WHILE, cond, body);
        }

for_stmt =
    'for' - ( src:array_var | src:list_expr | src:qw | src:twargs ) - '{' - body:statementlist - '}' { $$.set(kiji::NODE_FOR, src, body); }
    | 'for' - ( src:array_var | src:list_expr | src:qw | src:twargs ) - body:lambda { $$.set(kiji::NODE_FOR, src, body); }

unless_stmt = 'unless' - cond:expr - '{' - body:statementlist - '}' {
            $$.set(kiji::NODE_UNLESS, cond, body);
        }

if_stmt = 'if' - if_cond:expr - '{' - if_body:statementlist - '}' {
            $$.set(kiji::NODE_IF, if_cond, if_body);
            if_cond=$$;
        }
        (
            ws+ 'elsif' - elsif_cond:expr - '{' - elsif_body:statementlist - '}' {
                // elsif_body.change_type(kiji::NODE_ELSIF);
                $$.set(kiji::NODE_ELSIF, elsif_cond, elsif_body);
                // if_cond.push_child(elsif_cond);
                if_cond.push_child($$);
            }
        )*
        (
            ws+ 'else' ws+ - '{' - else_body:statementlist - '}' {
                else_body.change_type(kiji::NODE_ELSE);
                if_cond.push_child(else_body);
            }
        )? { $$=if_cond; }

postfix_if_stmt = body:normal_stmt - 'if' - cond:expr { $$.set(kiji::NODE_IF, cond, body); }

postfix_unless_stmt = body:normal_stmt - 'unless' - cond:expr { $$.set(kiji::NODE_UNLESS, cond, body); }

postfix_for_stmt = body:normal_stmt - 'for' - ( src:array_var | src:list_expr | src:qw | src:twargs ) { $$.set(kiji::NODE_FOR, src, body); }

# FIXME: simplify the code
bind_stmt =
          e1:my - ':=' - e2:list_expr { $$.set(kiji::NODE_BIND, e1, e2); }
        | e3:my - ':=' - e4:expr { $$.set(kiji::NODE_BIND, e3, e4); }
        | e5:expr { $$ = e5; }

list_expr =
    (a:methodcall_expr { $$.set(kiji::NODE_LIST, a); a = $$; }
        (- ','  - b:methodcall_expr { a.push_child(b); $$ = a; } )+
    ) { $$=a }

paren_args = '(' - a:args - ')' { $$=a; }
           | '(' - ')' { $$.set_children(kiji::NODE_ARGS); }

args =
    (
        s1:expr {
            $$.set(kiji::NODE_ARGS, s1);
            s1 = $$;
        }
        ( - ',' - s2:expr {
            s1.push_child(s2);
            $$ = s1;
        } )*
    )
    | '' { $$.set_children(kiji::NODE_ARGS); }
    # ↑ it makes slower?

expr = sequencer_expr

# TODO
sequencer_expr = loose_or_expr

loose_or_expr =
    f1:loose_and_expr (
        - 'or' - f2:loose_and_expr { $$.set(kiji::NODE_LOGICAL_AND, f1, f2); f1=$$; }
    )* { $$=f1; }

loose_and_expr =
    f1:list_prefix_expr (
        - 'and' - f2:list_prefix_expr { $$.set(kiji::NODE_LOGICAL_AND, f1, f2); f1=$$; }
    )* { $$=f1; }

list_prefix_expr =
    (v:variable - ':=' - e:loose_not_expr) { $$.set(kiji::NODE_BIND, v, e); }
    | loose_not_expr

loose_not_expr =
    'not' - f1:conditional_expr { $$.set(kiji::NODE_NOT, f1); }
    | f1:conditional_expr { $$=f1 }

conditional_expr = e1:tight_or - '??' - e2:tight_or - '!!' - e3:tight_or { $$.set(kiji::NODE_CONDITIONAL, e1, e2, e3); }
                | tight_or

tight_or = f1:tight_and (
        - '||' - f2:tight_and { $$.set(kiji::NODE_LOGICAL_OR, f1, f2); f1 = $$; }
    )* { $$ = f1; }

tight_and = f1:cmp_expr (
        - '&&' - f2:cmp_expr { $$.set(kiji::NODE_LOGICAL_AND, f1, f2); f1 = $$; }
    )* { $$ = f1; }

cmp_expr = f1:methodcall_expr (
          - '==' - f2:methodcall_expr { $$.set(kiji::NODE_EQ, f1, f2); f1=$$; }
        | - '!=' - f2:methodcall_expr { $$.set(kiji::NODE_NE, f1, f2); f1=$$; }
        | - '<'  - f2:methodcall_expr { $$.set(kiji::NODE_LT, f1, f2); f1=$$; }
        | - '<=' - f2:methodcall_expr { $$.set(kiji::NODE_LE, f1, f2); f1=$$; }
        | - '>'  - f2:methodcall_expr { $$.set(kiji::NODE_GT, f1, f2); f1=$$; }
        | - '>=' - f2:methodcall_expr { $$.set(kiji::NODE_GE, f1, f2); f1=$$; }
        | - 'eq' - f2:methodcall_expr { $$.set(kiji::NODE_STREQ, f1, f2); f1=$$; }
        | - 'ne' - f2:methodcall_expr { $$.set(kiji::NODE_STRNE, f1, f2); f1=$$; }
        | - 'gt' - f2:methodcall_expr { $$.set(kiji::NODE_STRGT, f1, f2); f1=$$; }
        | - 'ge' - f2:methodcall_expr { $$.set(kiji::NODE_STRGE, f1, f2); f1=$$; }
        | - 'lt' - f2:methodcall_expr { $$.set(kiji::NODE_STRLT, f1, f2); f1=$$; }
        | - 'le' - f2:methodcall_expr { $$.set(kiji::NODE_STRLE, f1, f2); f1=$$; }
    )* {
        $$ = f1;
    }

methodcall_expr =
    a1:atpos_expr (
        '.' a2:ident { $$.set(kiji::NODE_METHODCALL, a1, a2); a1=$$; }
        (
            a3:paren_args { a1.push_child(a3) }
        )?
    )? { $$=a1; }

atpos_expr =
    f1:not_expr - '[' - f2:not_expr - ']' {
        $$.set(kiji::NODE_ATPOS, f1, f2);
    }
    | not_expr

funcall =
    (i:ident - '(' - a:args - ')') {
        $$.set(kiji::NODE_FUNCALL, i, a);
    }

not_expr =
    ( '!' - a:add_expr ) { $$.set(kiji::NODE_NOT, a); }
    | add_expr

add_expr =
    l:multiplicative_expr (
          - '+' - r1:multiplicative_expr {
            $$.set(kiji::NODE_ADD, l, r1);
            l = $$;
          }
        | - '-' - r2:multiplicative_expr {
            $$.set(kiji::NODE_SUB, l, r2);
            l = $$;
          }
        | - '~' - r2:multiplicative_expr {
            $$.set(kiji::NODE_STRING_CONCAT, l, r2);
            l = $$;
          }
        | - '+|' - r:exponentiation_expr {
            $$.set(kiji::NODE_BIN_OR, l, r);
            l = $$;
        }
        | - '+^' - r:exponentiation_expr {
            $$.set(kiji::NODE_BIN_XOR, l, r);
            l = $$;
        }
    )* {
        $$ = l;
    }

multiplicative_expr =
    l:symbolic_unary (
        - '*' - r:symbolic_unary {
            $$.set(kiji::NODE_MUL, l, r);
            l = $$;
        }
        | - '/' - r:symbolic_unary {
            $$.set(kiji::NODE_DIV, l, r);
            l = $$;
        }
        | - '%' - r:symbolic_unary {
            $$.set(kiji::NODE_MOD, l, r);
            l = $$;
        }
        | - '+&' - r:symbolic_unary {
            $$.set(kiji::NODE_BIN_AND, l, r);
            l = $$;
        }
    )* {
        $$ = l;
    }

symbolic_unary =
    '+' - f1:exponentiation_expr { $$.set(kiji::NODE_UNARY_PLUS, f1); }
    | exponentiation_expr

exponentiation_expr = 
    f1:autoincrement_expr (
        - '**' - f2:autoincrement_expr {
            $$.set(kiji::NODE_POW, f1, f2);
            f1=$$;
        }
    )* {
        $$=f1;
    }

# ++, -- is not supported yet
autoincrement_expr = method_postfix_expr

method_postfix_expr = ( container:term '{' - k:term - '}' ) { $$.set(kiji::NODE_ATKEY, container, k); }
           | ( container:term '<' - k:ident - '>' ) { k.change_type(kiji::NODE_STRING); $$.set(kiji::NODE_ATKEY, container, k); }
           | ( container:term '(' - k:args - ')' ) { $$.set(kiji::NODE_FUNCALL, container, k); }
           | term

term = 
    ( '-' ( integer | dec_number) ) {
        $$.negate();
    }
    | integer
    | dec_number
    | string
    | '(' - e:expr  - ')' { $$ = e; }
    | '(' - l:list_expr  - ')' { $$ = l; }
    | variable
    | '$?LINE' { $$.set_integer(G->data.line_number); }
    | array
    | funcall
    | qw
    | twargs
    | hash
    | lambda
    | it_method

it_method = (
        '.' i:ident { $$.set(kiji::NODE_IT_METHODCALL, i); i=$$; }
        (
            a:paren_args { i.push_child(a); }
        )?
    ) { $$=i; }

ident = < [a-zA-Z] [a-zA-Z0-9]* ( ( '_' | '-') [a-zA-Z0-9]+ )* > {
    $$.set_ident(yytext, yyleng);
}


hash = '{' -
    p1:pair { $$.set(kiji::NODE_HASH, p1); p1=$$; } ( -  ',' - p2:pair { p1.push_child(p2); $$=p1; } )*
    - '}' { $$=p1; }

pair = k:hash_key - '=>' - v:expr { $$.set(kiji::NODE_PAIR, k, v); }

hash_key =
    < [a-zA-Z0-9_]+ > { $$.set_string(yytext, yyleng); }
    | string

twargs='@*ARGS' { $$.set_clargs(); }

qw =
    '<<' - qw_list - '>>'
    | '<' - qw_list - '>'

qw_list =
        a:qw_item { $$.set(kiji::NODE_LIST, a); a = $$; }
        ( ws+ b:qw_item { a.push_child(b); $$ = a; } )*
        { $$=a; }

# I want to use [^ ] but greg does not support it...
# https://github.com/nddrylliog/greg/issues/12
qw_item = < [a-zA-Z0-9_]+ > { $$.set_string(yytext, yyleng); }

funcdef =
    'sub' - i:ident - '(' - p:params? - ')' - b:block {
        if (p.is_undefined()) {
            p.set_children(kiji::NODE_PARAMS);
        }
        $$.set(kiji::NODE_FUNC, i, p, b);
    }

lambda =
    '->' - p:params? - b:block {
        if (p.is_undefined()) {
            p.set_children(kiji::NODE_PARAMS);
        }
        $$.set(kiji::NODE_LAMBDA, p, b);
    }

params =
    v:term { $$.set(kiji::NODE_PARAMS, v); v=$$; }
    ( - ',' - v1:term { v.push_child(v1); $$=v; } )*
    { $$=v; }

block = 
    ('{' - s:statementlist - '}') { $$=s; }
    | ('{' - '}' ) { $$.set_children(kiji::NODE_STATEMENTS); }

array =
    '[' e:expr { $$.set(kiji::NODE_ARRAY, e); e=$$; } ( ',' e2:expr { e.push_child(e2); $$=e; } )* ']' { $$=e; }
    | '[' - ']' { $$.set_children(kiji::NODE_ARRAY); }

my = 'my' ws v:variable { $$.set(kiji::NODE_MY, v); }

variable = scalar | array_var

array_var = < '@' [a-zA-Z_] [a-zA-Z0-9]* > { $$.set_variable(yytext, yyleng); }

scalar = < '$' [a-zA-Z_] [a-zA-Z0-9]* > { assert(yyleng > 0); $$.set_variable(yytext, yyleng); }

#  <?MARKED('endstmt')>
#  <?terminator>
eat_terminator =
    (';' -) | end-of-file

dec_number =
    <([.][0-9]+)> {
    $$.set_number(yytext);
}
    | <([0-9]+ '.' [0-9]+)> {
    $$.set_number(yytext);
}
    | <([0-9_]+)> {
    $$.set_integer(yytext, yyleng, 10);
}

integer =
    '0b' <[01_]+> {
    $$.set_integer(yytext, yyleng, 2);
}
    | '0d' <[0-9]+> {
    $$.set_integer(yytext, yyleng, 10);
}
    | '0x' <[0-9a-f_]+> {
    $$.set_integer(yytext, yyleng, 16);
}
    | '0o' <[0-7]+> {
    $$.set_integer(yytext, yyleng, 8);
}

string = dq_string | sq_string

dq_string = '"' { $$.init_string(); } (
        "\n" { G->data.line_number++; $$.append_string("\n", 1); }
        | < [^"\\\n]+ > { $$.append_string(yytext, yyleng); }
        | esc 'a' { $$.append_string("\a", 1); }
        | esc 'b' { $$.append_string("\b", 1); }
        | esc 't' { $$.append_string("\t", 1); }
        | esc 'r' { $$.append_string("\r", 1); }
        | esc 'n' { $$.append_string("\n", 1); }
        | esc '"' { $$.append_string("\"", 1); }
        | ( esc 'x' (
                  '0'? < ( [a-fA-F0-9] [a-fA-F0-9] ) >
            | '[' '0'? < ( [a-fA-F0-9] [a-fA-F0-9] ) > ']' )
        ) {
            $$.append_string_from_hex(yytext, yyleng);
        }
        | esc 'o' < '0'? [0-7] [0-7] > {
            $$.append_string_from_oct(yytext, yyleng);
        }
        | esc 'o['
             '0'? < [0-7] [0-7] > { $$.append_string_from_oct(yytext, yyleng); } (
            ',' '0'? < [0-7] [0-7] > { $$.append_string_from_oct(yytext, yyleng); }
        )* ']'
        | esc esc { $$.append_string("\\", 1); }
    )* '"'

esc = '\\'

sq_string = "'" { $$.init_string(); } (
        "\n" { G->data.line_number++; $$.append_string("\n", 1); }
        | < [^'\\\n]+ > { $$.append_string(yytext, yyleng); }
        | esc "'" { $$.append_string("'", 1); }
        | esc esc { $$.append_string("\\", 1); }
        | < esc . > { $$.append_string(yytext, yyleng); }
    )* "'"

# TODO

comment= '#' [^\n]* end-of-line

# white space
ws = ' ' | '\f' | '\v' | '\t' | '\205' | '\240' | end-of-line
    | comment
- = ws*
end-of-line = ( '\r\n' | '\n' | '\r' ) {
    G->data.line_number++;
}
end-of-file = !'\0'

%%
