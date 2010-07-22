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
 * @file sg_logging_prototypes.h
 *
 * @details Routines for logging.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LOGGING_PROTOTYPES_H
#define H_SG_LOGGING_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

// Emit message to all loggers, including those set to log level "quiet":
void SG_log_urgent(SG_context *, const char * szFormat, ...);
void SG_vlog_urgent(SG_context *, const char * szFormat, va_list);

// Emit message to loggers that are set at log level "normal" or "verbose":
void SG_log(SG_context *, const char * szFormat, ...);
void SG_vlog(SG_context *, const char * szFormat, va_list);

// Emit message to loggers that are set at log level "verbose":
void SG_log_debug(SG_context *, const char * szFormat, ...);
void SG_vlog_debug(SG_context *, const char * szFormat, va_list);

// Note: do not wrap calls to these in SG_ERR_IGNORE or it will hide the error.
void SG_log_current_error(SG_context *);
void SG_log_current_error_urgent(SG_context *);
void SG_log_current_error_debug(SG_context *);

// A logger is a function that accepts messages emitted by sg_logging, together with a
// context pointer that will be passed to the function along with each message.
// SG_logging__register_logger registers your logger so that it will be sent messages
// appropriate for the output level.
//
// The logger you register must be unique, or a SG_ERR_ALREADY_INITIALIZED error will
// be returned. Note this does not mean that the callback function need be unique, but
// rather that the <callback function, context pointer> pair must be unique.
void SG_logging__register_logger(
	SG_context * pCtx,
	SG_logger_callback,
	void * pContext,
	SG_logger_output_level);

// A logger is a function that accepts messages emitted by sg_logging, together with a
// context pointer that will be passed to the function along with each message.
// SG_logging__unregister_logger unregisters your logger so that it will no longer be
// sent any messages.
void SG_logging__unregister_logger(
	SG_context * pCtx,
	SG_logger_callback,
	void * pLoggerData);

// A function to convert from a string to a log level? Handy!:
SG_enumeration SG_logging__enumerate_log_level(const char *);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_LOGGING_PROTOTYPES_H
