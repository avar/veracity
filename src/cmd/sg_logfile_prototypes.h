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
 * @file sg_logfile_prototypes.h
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LOGFILE_PROTOTYPES_H
#define H_SG_LOGFILE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////


void SG_logfile__alloc(SG_context *, SG_pathname** ppPath, SG_logfile **);

void SG_logfile__nullfree(SG_context *, SG_logfile **);

void SG_logfile__write(SG_context *, SG_logfile * pThis, const char *);

// If you've already got a time associated with what you're writing,
// pass it in so we don't have to fetch it again (in order to check
// whether a rollover is needed).
//
// TODO REVIEW: Jeff says: SG_time is a structure.  We shouldn't be passing it by *value*
//                         to this function.  We should pass "SG_int64 iTime" instead.
//                         See Jira:SPRAWL-79.
void SG_logfile__write__time(SG_context *, SG_logfile * pThis, const char *, SG_time);


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_LOGFILE_PROTOTYPES_H
