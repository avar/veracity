/*
Copyright 2010 SourceGear, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/**
 * @file sg_error_prototypes.h
 *
 * @details Declarations of error-related routines.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_ERROR_PROTOTYPES_H
#define H_SG_ERROR_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// we ASSUME that each of the different error domains (errno, GetLastError, COM, SQLite, etc)
// all define 0 to be no error within their domain of values.  so we must strip off the domain
// indicator when testing for _OK.

#define SG_IS_OK(e)					(SG_ERR_GENERIC_VALUE(e) == 0)
#define SG_IS_ERROR(e)				(SG_ERR_GENERIC_VALUE(e) != 0)

//////////////////////////////////////////////////////////////////
// SG_error__get_message -- like strerror().
// caller must provide buffer and set buffer length.  error message
// will be truncated to fit in the given buffer.  (we recommend using
// SG_ERROR_BUFFER_SIZE.)  A pointer to the given buffer is returned
// for convenience.

#define SG_ERROR_BUFFER_SIZE			(1024)

extern void SG_error__get_message(SG_error err,
									char * pBuf,
									SG_uint32 lenBuf);

//////////////////////////////////////////////////////////////////

// Set an error state and goto fail.
#define SG_ERR_THROW(errnum)		SG_STATEMENT(	SG_context__err__generic(pCtx, errnum, __FILE__, __LINE__);	\
													goto fail;													)

// Set an error state and return.
#define SG_ERR_THROW_RETURN(errnum)	SG_STATEMENT(	SG_context__err__generic(pCtx, errnum, __FILE__, __LINE__);	\
													return;														)

// Set an error state with a description and goto fail.
// 'info' should be a parenthesized list: pCtx then printf args.
#define SG_ERR_THROW2(errnum,info)	SG_STATEMENT(	SG_context__err__generic(pCtx, errnum, __FILE__, __LINE__);	\
													SG_context__err_set_description info;						\
													goto fail;													)

// Set an error state with a description and return.
// 'info' should be a parenthesized list: pCtx then printf args.
#define SG_ERR_THROW2_RETURN(errnum,info)	SG_STATEMENT(	SG_context__err__generic(pCtx, errnum, __FILE__, __LINE__);	\
															SG_context__err_set_description info;						\
															return;														)

// Goto fail with the current, pre-existing error state.  Add stack frame to error info.
#define SG_ERR_RETHROW				SG_STATEMENT(	SG_context__err_stackframe_add(pCtx, __FILE__, __LINE__);	\
													goto fail;													)

// Return with the current, pre-existing error state.  Add stack frame to error info.
#define SG_ERR_RETHROW_RETURN		SG_STATEMENT(	SG_context__err_stackframe_add(pCtx, __FILE__, __LINE__);	\
													return;														)

// Goto fail with the current, pre-existing error state.  Add stack frame and description to error info.
#define SG_ERR_RETHROW2(info)		SG_STATEMENT(	SG_context__err_stackframe_add(pCtx, __FILE__, __LINE__);		\
													SG_context__err_set_description info;							\
													goto fail;														)

//////////////////////////////////////////////////////////////////

// If the provided argument is null, throws SG_ERR_INVALIDARG and jumps to fail.
#define SG_NULLARGCHECK(argname)		SG_STATEMENT(	if (!argname)										\
														{													\
															(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
																__FILE__, __LINE__, #argname " is null");	\
															goto fail;										\
														}													)

// If the provided argument is null, throws SG_ERR_INVALIDARG and returns.
#define SG_NULLARGCHECK_RETURN(argname)	SG_STATEMENT(	if (!argname)										\
														{													\
															(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
															__FILE__, __LINE__, #argname " is null");		\
															return;											\
														}													)

// Throws SG_ERR_INVALIDARG and jumps to fail if (!pp || !*pp)
#define SG_NULL_PP_CHECK(pp)			SG_STATEMENT(	if (!pp)											\
														{													\
															(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
															__FILE__, __LINE__, #pp " is null");			\
															goto fail;										\
														}													\
														else if (!*pp)										\
														{													\
															(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
															__FILE__, __LINE__, "*" #pp " is null");		\
															goto fail;										\
														}													)

// Throws SG_ERR_INVALIDARG and returns if (!pp || !*pp)
#define SG_NULL_PP_CHECK_RETURN(pp)			SG_STATEMENT(	if (!pp)											\
															{													\
																(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
																__FILE__, __LINE__, #pp " is null");			\
																return;											\
															}													\
															else if (!*pp)										\
															{													\
																(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
																__FILE__, __LINE__, "*" #pp " is null");		\
																return;											\
															}													)

#define SG_NONEMPTYCHECK(sz)				SG_NULL_PP_CHECK(sz)
#define SG_NONEMPTYCHECK_RETURN(sz)			SG_NULL_PP_CHECK_RETURN(sz)

// Throws SG_ERR_INVALIDARG and jumps to fail if expr is false.
#define SG_ARGCHECK(expr,argname)			SG_STATEMENT(	if (!(expr))										\
															{													\
																(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
																__FILE__, __LINE__, #argname " is invalid: "	\
																#expr " is false.");							\
																goto fail;										\
															}													)

// Throws SG_ERR_INVALIDARG and returns if expr is false.
#define SG_ARGCHECK_RETURN(expr,argname)	SG_STATEMENT(	if (!(expr))										\
															{													\
																(void) SG_context__err(pCtx, SG_ERR_INVALIDARG,	\
																__FILE__, __LINE__, #argname " is invalid: "	\
																#expr " is false.");							\
																return;											\
															}													)

//////////////////////////////////////////////////////////////////

/**
 * This macro allows for an abbreviated syntax when checking error
 * conditions.  Instead of:
 *
 * SG_foo(pCtx, pidRepo, buf);
 * if (SG_context__has_err(pCtx) goto fail;
 *
 * we can type;:
 *
 * SG_ERR_CHECK(  SG_foo(pCtx, pidRepo, buf) );
 *
 * This is not an assert.  It is safe to use in non-debug code,
 * as it does not 'disappear' when compiling in release mode
 * like an assert would.  This is for run-time error checking.
 *
 * The expr argument is evaluated only once, so this is safe
 * to use with a function call as the argument.
 *
 * This macro assumes that a local variable (SG_context* pCtx) exists.
 * It also assumes that a label called 'fail' exists.
 */
#define SG_ERR_CHECK(expr)			SG_STATEMENT(	expr;														\
													if (SG_CONTEXT__HAS_ERR(pCtx))								\
													{	SG_context__err_stackframe_add(pCtx,__FILE__,__LINE__);	\
														goto fail;												\
													}															)

/**
 * Error check with additional logging information.
 *
 * 'info' should be a parenthesized list: pCtx then printf args.
 *
 * SG_ERR_CHECK2(  SG_changeset__load_from_repo(pCtx,pRepo,szHidCSet,&pcs),
 *                 (pCtx, "Loading changeset %s.",szHidCSet)  );
 *
 */
#define SG_ERR_CHECK2(expr,info)	SG_STATEMENT(	expr;														\
													if (SG_CONTEXT__HAS_ERR(pCtx))								\
													{	SG_context__err_set_description info;					\
														SG_context__err_stackframe_add(pCtx,__FILE__,__LINE__);	\
														goto fail;												\
													}															)
/**
 * This macro is like SG_ERR_CHECK(), but it RETURNS from the
 * function with the error code rather than just jumping to the
 * label.
 */
#define SG_ERR_CHECK_RETURN(expr)			SG_STATEMENT(	expr;														\
															if (SG_CONTEXT__HAS_ERR(pCtx))								\
															{	SG_context__err_stackframe_add(pCtx,__FILE__,__LINE__);	\
																return;													\
															}															)

 /**
 * Like SG_ERR_CHECK_RETURN but with additional logging info.
 */
#define SG_ERR_CHECK2_RETURN(expr,info)		SG_STATEMENT(	expr;														\
															if (SG_CONTEXT__HAS_ERR(pCtx))								\
															{	SG_context__err_set_description info ;					\
																SG_context__err_stackframe_add(pCtx,__FILE__,__LINE__);	\
																return;												\
															}															)

#define SG_ERR_RESET_THROW(errnum)				SG_STATEMENT( SG_context__err_reset(pCtx); SG_ERR_THROW(errnum); )
#define SG_ERR_RESET_THROW_RETURN(errnum)		SG_STATEMENT( SG_context__err_reset(pCtx); SG_ERR_THROW_RETURN(errnum); )
#define SG_ERR_RESET_THROW2(errnum,info)		SG_STATEMENT( SG_context__err_reset(pCtx); SG_ERR_THROW2(errnum,info); )
#define SG_ERR_RESET_THROW2_RETURN(errnum,info) SG_STATEMENT( SG_context__err_reset(pCtx); SG_ERR_THROW2_RETURN(errnum,info); )

// If there's an existing error, goto fail.  Add stack frame to error info.
#define SG_ERR_CHECK_CURRENT		SG_STATEMENT(	if ( SG_CONTEXT__HAS_ERR(pCtx) )								\
													{																\
														SG_context__err_stackframe_add(pCtx, __FILE__, __LINE__);	\
														goto fail;													\
													}																)

// If there's an existing error, return.  Add stack frame to error info.
#define SG_ERR_CHECK_RETURN_CURRENT	SG_STATEMENT(	if ( SG_CONTEXT__HAS_ERR(pCtx) )								\
													{																\
														SG_context__err_stackframe_add(pCtx, __FILE__, __LINE__);	\
														return;													\
													}																)

//////////////////////////////////////////////////////////////////

#define SG_ERR_IGNORE(expr)			SG_STATEMENT(	SG_context__push_level(pCtx);								\
													expr;														\
													SG_context__pop_level(pCtx);								)

//////////////////////////////////////////////////////////////////

/**
* If in the error state indicated by if_old_errnum, set the error number to newerrnum
*/
#define SG_ERR_REPLACE(if_old_errnum, newerrnum)	SG_STATEMENT (  if (SG_context__err_equals(pCtx, if_old_errnum))					\
																		SG_context__err_replace(pCtx, newerrnum, __FILE__, __LINE__);	)

#define SG_ERR_REPLACE2(if_old_errnum,newerrnum,info)SG_STATEMENT(  if (SG_context__err_equals(pCtx, if_old_errnum))					\
																		SG_context__err_replace(pCtx, newerrnum, __FILE__, __LINE__);	\
																		SG_context__err_set_description info;							)

//////////////////////////////////////////////////////////////////

#define SG_ERR_DISCARD			SG_STATEMENT(  SG_context__err_reset(pCtx);  )
#define SG_ERR_DISCARD2(ctx)	SG_STATEMENT(  SG_context__err_reset(ctx);  )

//////////////////////////////////////////////////////////////////

// Disregard ignore_err, otherwise SG_ERR_CHECK.
#define SG_ERR_CHECK_CURRENT_DISREGARD(ignore_err)			SG_STATEMENT(	if (SG_context__err_equals(pCtx, ignore_err))	\
																				SG_context__err_reset(pCtx);				\
																			else											\
																				SG_ERR_CHECK_CURRENT;						);

// Disregard ignore_err, otherwise SG_ERR_CHECK_RETURN.
#define SG_ERR_CHECK_RETURN_CURRENT_DISREGARD(ignore_err)	SG_STATEMENT(	if (SG_context__err_equals(pCtx, ignore_err))	\
																				SG_context__err_reset(pCtx);				\
																			else											\
																				SG_ERR_CHECK_RETURN_CURRENT;				);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)

/**
 * A wrapper for the debug-only CRT assert() macro/function.  This calls assert()/abort() if the expression
 * is false.  We point the SG_ASSERT() macros at it so that SG_assert__debug() gets billed for code coverage
 * and the caller doesn't get dinged for not testing the assert failing.
 */
void SG_assert__debug(SG_bool bExpr, const char * pszExpr, const char * pszFile, int line);

/**
 * A version of ASSERT that is included only in debug code.
 */
#	if 1
#		define SG_ASSERT(expr)		SG_STATEMENT(  if (!(expr)) { SG_assert__debug( 0, #expr, __FILE__, __LINE__ ); }  )
#	else
#		define SG_ASSERT(expr)		SG_assert__debug( ((expr) != 0), #expr, __FILE__, __LINE__ )
#	endif

#else

/**
 * Do nothing for an SG_ASSERT in release code.
 */
#define SG_ASSERT(expr)

#endif

//////////////////////////////////////////////////////////////////

/**
 * A version of ASSERT that is included in RELEASE AND DEBUG code and plays nice with our error handling.
 *
 * You probably don't want to call this directly.  Use one of the SG_ASSERT_RELEASE_ macros.
 */
void SG_assert__release(SG_context* pCtx, const char * szExpr, const char * szFile, int line);

/**
 * Check the given expression.  Log the event.  Go to fail.
 * THIS DOES NOT CALL abort().  The check is performed in release and debug builds.
 *
 * This meshes nicely with our other error handling.  The actual error code
 * is set in SG_assert__release() so that you can override it and continue when in the
 * debugger.
 *
 * This macro assumes that a local variable (SG_context* pCtx) exists.
 * It also assumes that a label called 'fail' exists.
 */
#define SG_ASSERT_RELEASE_FAIL(expr)	SG_STATEMENT(	SG_bool bFail = !(expr);										\
														if (bFail)														\
														{	SG_assert__release(pCtx, #expr,__FILE__,__LINE__);			\
															if (SG_CONTEXT__HAS_ERR(pCtx))								\
																goto fail;												\
														}																)

/**
 * Check the given expression.  Log the event.  Return.
 * THIS DOES NOT CALL abort().  The check is performed in release and debug builds.
 *
 * This meshes nicely with our other error handling.  The actual error code
 * is set in SG_assert__release() so that you can override it and continue when in the
 * debugger.
 *
 * This macro assumes that a local variable (SG_context* pCtx) exists.
 */
#define SG_ASSERT_RELEASE_RETURN(expr)	SG_STATEMENT(	SG_bool bFail = !(expr);										\
														if (bFail)														\
														{	SG_assert__release(pCtx, #expr,__FILE__,__LINE__);			\
															if (SG_CONTEXT__HAS_ERR(pCtx))								\
																return;													\
														}																)

/**
 * Check the given expression.  Log the event with an extra message.  Go to fail.
 * THIS DOES NOT CALL abort().    The check is performed in release and debug builds.
 *
 * 'info' should be a parenthesized list: pCtx and printf args.
 *
 * This meshes nicely with our other error handling.  The actual error code
 * is set in SG_assert__release() so that you can override it and continue when in the
 * debugger.
 *
 * This macro assumes that a local variable (SG_context* pCtx) exists.
 * It also assumes that a label called 'fail' exists.
 */
#define SG_ASSERT_RELEASE_FAIL2(expr,info)	SG_STATEMENT(	SG_bool bFail = !(expr);									\
															if (bFail)													\
															{	SG_assert__release(pCtx, #expr,__FILE__,__LINE__);		\
																if (SG_CONTEXT__HAS_ERR(pCtx))							\
																{	SG_context__err_set_description info;				\
																	goto fail;											\
																}														\
															}															)

/**
 * Check the given expression.  Log the event.  Return.
 * THIS DOES NOT CALL abort().  The check is performed in release and debug builds.
 *
 * 'info' should be a parenthesized list: pCtx and printf args.
 *
 * This meshes nicely with our other error handling.  The actual error code
 * is set in SG_assert__release() so that you can override it and continue when in the
 * debugger.
 *
 * This macro assumes that a local variable (SG_context* pCtx) exists.
 */
#define SG_ASSERT_RELEASE_RETURN2(expr,info)	SG_STATEMENT(	SG_bool bFail = !(expr);								\
															if (bFail)													\
															{	SG_assert__release(pCtx, #expr,__FILE__,__LINE__);		\
																if (SG_CONTEXT__HAS_ERR(pCtx))							\
																{	SG_context__err_set_description info;				\
																	return;												\
																}														\
															}															)


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_ERROR_PROTOTYPES_H
