#ifndef KIJI_FRAME_H_
#define KIJI_FRAME_H_
/* vim:ts=2:sw=2:tw=0: */

#pragma once

#include <sstream>
#include "handy.h"

enum Kiji_variable_type_t {
  KIJI_VARIABLE_TYPE_MY,
  KIJI_VARIABLE_TYPE_OUR
};

typedef struct _KijiFrame {
  MVMString **package_variables;
  MVMuint32 num_package_variables;
  MVMStaticFrame frame; // frame itself
  struct _KijiFrame* outer;
} KijiFrame;

void Kiji_frame_push_handler(KijiFrame*self, MVMFrameHandler* handler);
int Kiji_frame_push_local_type(KijiFrame* self, MVMuint16 reg_type);
uint16_t Kiji_frame_get_local_type(KijiFrame*self, int n);
int Kiji_frame_push_lexical(KijiFrame*self, MVMThreadContext *tc, MVMString*name, MVMuint16 type);
void Kiji_frame_push_pkg_var(KijiFrame* self, MVMString *name);
void Kiji_frame_set_outer(KijiFrame *self, KijiFrame*framef);
bool Kiji_frame_find_lexical_by_name(KijiFrame* frame_, MVMThreadContext* tc, const MVMString* name, int *lex_no, int *outer);
Kiji_variable_type_t Kiji_find_variable_by_name(KijiFrame *f, MVMThreadContext* tc, MVMString * name, int *lex_no, int *outer);

#endif /* KIJI_FRAME_H_ */
