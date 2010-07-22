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
 * @file sg_logging_typedefs.h
 *
 * @details Typedefs for working with SG_logging__ routines.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LOGGING_TYPEDEFS_H
#define H_SG_LOGGING_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

enum _sg_logger_output_level
{
	// Level at which only errors will be logged.
	SG_LOGGER_OUTPUT_LEVEL_QUIET = 0,

	// Level at which only errors and some high-level information will be logged.
	SG_LOGGER_OUTPUT_LEVEL_NORMAL = 1,

	// Level at which ighly detailed output with all debug statements will be logged.
	SG_LOGGER_OUTPUT_LEVEL_VERBOSE = 2
};
typedef enum _sg_logger_output_level SG_logger_output_level;

enum _sg_log_message_level
{
	// For messages that are to be logged by loggers set to any level, including quiet.
	SG_LOG_MESSAGE_LEVEL_URGENT = 0,

	// For messages that are to be logged by loggers set to normal or verbose output levels.
	SG_LOG_MESSAGE_LEVEL_NORMAL = 1,

	// For messages that are to be logged only by loggers set to the verbose output level.
	SG_LOG_MESSAGE_LEVEL_DEBUG = 2
};
typedef enum _sg_log_message_level SG_log_message_level;

// A logger is a function that accepts messages emitted by sg_logging, together with a
// context pointer that will be passed to the function along with each message.
typedef void SG_logger_callback(
	SG_context * pCtx,
	const char * message,  // The body of the message
	SG_log_message_level message_level, // The message level of the message
	SG_int64 timestamp, // The time the message was emitted, in "microseconds_since_1970_utc".
	void * pContext); // The context pointer that you provided when you registered the logger.

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_LOGGING_TYPEDEFS_H
