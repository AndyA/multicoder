/* mc_model.h */

#ifndef MODEL_H_
#define MODEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <jd_pretty.h>

  jd_var *mc_model_vget(jd_var *v, jd_var *fallback, const char *path, va_list ap);
  jd_var *mc_model_get(jd_var *v, jd_var *fallback, const char *path, ...);
  jd_int mc_model_get_int(jd_var *v, jd_int fallback, const char *path, ...);
  double mc_model_get_real(jd_var *v, double fallback, const char *path, ...);
  const char *mc_model_get_str(jd_var *v, const char *fallback, const char *path, ...);

  jd_var *mc_model_load_string(jd_var *out, FILE *f);
  jd_var *mc_model_load_json(jd_var *out, FILE *f);
  jd_var *mc_model_load_file(jd_var *out, const char *fn);
  jd_var *mc_model_new(jd_var *stash, jd_var *name);
  jd_var *mc_model_load(jd_var *stash);

  jd_var *mc_model_multi_new(jd_var *stash, jd_var *names);
  jd_var *mc_model_multi_load(jd_var *out, jd_var *stash);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
