
#ifndef __sandboxutils_marshal_MARSHAL_H__
#define __sandboxutils_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:VOID (sandboxutilsmarshals.list:1) */
#define sandboxutils_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:INT (sandboxutilsmarshals.list:2) */
#define sandboxutils_marshal_VOID__INT	g_cclosure_marshal_VOID__INT

/* VOID:INT,INT (sandboxutilsmarshals.list:3) */
extern void sandboxutils_marshal_VOID__INT_INT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

/* INT:VOID (sandboxutilsmarshals.list:4) */
extern void sandboxutils_marshal_INT__VOID (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:STRING,INT,INT,BOOLEAN (sandboxutilsmarshals.list:5) */
extern void sandboxutils_marshal_VOID__STRING_INT_INT_BOOLEAN (GClosure     *closure,
                                                               GValue       *return_value,
                                                               guint         n_param_values,
                                                               const GValue *param_values,
                                                               gpointer      invocation_hint,
                                                               gpointer      marshal_data);

G_END_DECLS

#endif /* __sandboxutils_marshal_MARSHAL_H__ */

