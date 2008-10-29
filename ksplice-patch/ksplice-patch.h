#ifndef _KSPLICE_PATCH_H
#define _KSPLICE_PATCH_H

enum ksplice_option_type {
	KSPLICE_OPTION_ASSUME_RODATA,
};

struct ksplice_option {
	enum ksplice_option_type type;
	const void *target;
};

#ifdef __KERNEL__
#ifndef __used
#define __used __attribute_used__
#endif

#define ksplice_call_int(name, fn) \
	static typeof(int (*)(void)) __ksplice_##name##_##fn __used \
	__attribute__((__section__(".ksplice_call_" #name))) = fn

#define ksplice_call_void(name, fn) \
	static typeof(void (*)(void)) __ksplice_##name##_##fn __used \
	__attribute__((__section__(".ksplice_call_" #name))) = fn

#define ksplice_pre_apply(fn) ksplice_call_int(pre_apply, fn)
#define ksplice_check_apply(fn) ksplice_call_int(check_apply, fn)
#define ksplice_apply(fn) ksplice_call_void(apply, fn)
#define ksplice_post_apply(fn) ksplice_call_void(post_apply, fn)
#define ksplice_fail_apply(fn) ksplice_call_void(fail_apply, fn)

#define ksplice_pre_reverse(fn) ksplice_call_int(pre_reverse, fn)
#define ksplice_check_reverse(fn) ksplice_call_int(check_reverse, fn)
#define ksplice_reverse(fn) ksplice_call_void(reverse, fn)
#define ksplice_post_reverse(fn) ksplice_call_void(post_reverse, fn)
#define ksplice_fail_reverse(fn) ksplice_call_void(fail_reverse, fn)

#define ksplice_assume_rodata(obj)				\
	const struct ksplice_option				\
	__used __attribute__((__section__(".ksplice_options")))	\
	assume_rodata_##obj = {					\
		.type = KSPLICE_OPTION_ASSUME_RODATA,		\
		.target = (const void *)&obj,			\
	}
#endif /* __KERNEL__ */

#endif /* _KSPLICE_PATCH_H */
