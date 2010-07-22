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
 *
 * @file sg_mutex_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MUTEX_PROTOTYPES_H
#define H_SG_MUTEX_PROTOTYPES_H

BEGIN_EXTERN_C;

int SG_mutex__init(SG_mutex* pp);
void SG_mutex__destroy(SG_mutex* p);
int SG_mutex__lock(SG_mutex* p);
int SG_mutex__trylock(SG_mutex* p, SG_bool* pb);
int SG_mutex__unlock(SG_mutex* p);

END_EXTERN_C;

#endif//H_SG_MUTEX_PROTOTYPES_H

