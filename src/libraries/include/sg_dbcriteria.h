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

#ifndef H_SG_DBCRITERIA_H
#define H_SG_DBCRITERIA_H

BEGIN_EXTERN_C

#define SG_CRIT_NDX_LEFT 0
#define SG_CRIT_NDX_OP 1
#define SG_CRIT_NDX_RIGHT 2

void SG_dbcriteria__check_one_record(SG_context*, const SG_varray* pcrit, SG_dbrecord* prec, SG_bool* pbResult);

END_EXTERN_C

#endif
