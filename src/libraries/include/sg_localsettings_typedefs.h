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
 * @file sg_localsettings_typedefs.h
 *
 * @details Typedefs for working with SG_localsettings__ routines.
 */

#ifndef H_SG_LOCALSETTINGS_TYPEDEFS_H
#define H_SG_LOCALSETTINGS_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////////


/*
 * Here are some #define's for the names of local settings.
 *
 * These can be used to specify the setting when calling an initialize,
 * get, set, or reset function in SG_localsettings. (The parameter name
 * is always pSettingName.)
 *
 * This is not meant to be comprehensive list of all settings, just
 * those that are used by sglib and are relatively public. Please make
 * #defines for client-specific settings elsewhere, or else just use
 * plain old strings without #defines.
 */
#define SG_LOCALSETTING__IGNORES                "ignores"
#define SG_LOCALSETTING__NEWREPO_DRIVER         "new_repo/driver"
#define SG_LOCALSETTING__NEWREPO_CONNECTSTRING  "new_repo/connect_string"
#define SG_LOCALSETTING__NEWREPO_HASHMETHOD     "new_repo/hash_method"
#define SG_LOCALSETTING__SSI_DIR                "server/files"
#define SG_LOCALSETTING__SERVER_HOSTNAME        "server/hostname"
#define SG_LOCALSETTING__USERID                 "whoami/userid"
#define SG_LOCALSETTING__USEREMAIL              "whoami/email"
#define SG_LOCALSETTING__LOG_PATH               "log/path"
#define SG_LOCALSETTING__LOG_LEVEL              "log/level"
#define SG_LOCALSETTING__DIFF_2_PROGRAM			"diff/2/program"
#define SG_LOCALSETTING__DIFF_2_ARGUMENTS		"diff/2/arguments"

#define SG_LOCALSETTING__SCOPE__DEFAULT  "/default"
#define SG_LOCALSETTING__SCOPE__MACHINE  "/machine"
#define SG_LOCALSETTING__SCOPE__ADMIN    "/admin"
#define SG_LOCALSETTING__SCOPE__REPO     "/repo"
#define SG_LOCALSETTING__SCOPE__INSTANCE "/instance"

#define SG_LOCALSETTING__DIFF_SUBS__LEFT_PATH	"%LEFT_PATH%"
#define SG_LOCALSETTING__DIFF_SUBS__RIGHT_PATH	"%RIGHT_PATH%"
#define SG_LOCALSETTING__DIFF_SUBS__LEFT_LABEL	"%LEFT_LABEL%"
#define SG_LOCALSETTING__DIFF_SUBS__RIGHT_LABEL	"%RIGHT_LABEL%"
//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_LOCALSETTINGS_TYPEDEFS_H
