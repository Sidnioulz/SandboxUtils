
#ifndef __sandboxutils_marshal_MARSHAL_H__
#define __sandboxutils_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:STRING,INT,INT,BOOLEAN (sandboxutilsmarshals.list:1) */
extern void sandboxutils_marshal_VOID__STRING_INT_INT_BOOLEAN (GClosure     *closure,
                                                               GValue       *return_value,
                                                               guint         n_param_values,
                                                               const GValue *param_values,
                                                               gpointer      invocation_hint,
                                                               gpointer      marshal_data);

G_END_DECLS

#endif /* __sandboxutils_marshal_MARSHAL_H__ */
