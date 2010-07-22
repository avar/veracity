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

/*
 * sg_status_formatter.h
 *
 *  Created on: Jun 8, 2010
 *      Author: jeremy
 */

#ifndef SG_STATUS_FORMATTER_H_
#define SG_STATUS_FORMATTER_H_

void SG_status_formatter__vhash_to_string(SG_context * pCtx, SG_vhash * pvhTreediffs, SG_varray * pvaConflicts, SG_string ** pstrOutput);

#endif /* SG_STATUS_FORMATTER_H_ */
