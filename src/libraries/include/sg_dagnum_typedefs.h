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
 * @file sg_dagnum_typedefs.h
 *
 */

#ifndef H_SG_DAGNUM_TYPEDEFS_H
#define H_SG_DAGNUM_TYPEDEFS_H

BEGIN_EXTERN_C;

#define SG_DAGNUM__BUF_MAX__HEX 11
#define SG_DAGNUM__BUF_MAX__DEC 11
#define SG_DAGNUM__BUF_MAX__NAME 64

#define SG_DAGNUM__BIT__DB                  (0x01000000)
#define SG_DAGNUM__BIT__ADMIN               (0x02000000)
#define SG_DAGNUM__BIT__AUDIT               (0x04000000)
#define SG_DAGNUM__BIT__SINGLE_RECTYPE      (0x08000000)
#define SG_DAGNUM__BIT__NO_RECID            (0x00100000)
#define SG_DAGNUM__BIT__HARDWIRED_TEMPLATE  (0x00200000)

#define SG_DAGNUM__IS_DB(x) ( ((x) & SG_DAGNUM__BIT__DB) || ((x) & SG_DAGNUM__BIT__AUDIT) )
#define SG_DAGNUM__IS_AUDIT(x) ( ((x) & SG_DAGNUM__BIT__AUDIT)  )
#define SG_DAGNUM__IS_SINGLE_RECTYPE(x) ( ((x) & SG_DAGNUM__BIT__AUDIT) || ((x) & SG_DAGNUM__BIT__SINGLE_RECTYPE) )
#define SG_DAGNUM__IS_NO_RECID(x) ( ((x) & SG_DAGNUM__BIT__AUDIT) || ((x) & SG_DAGNUM__BIT__NO_RECID) )
#define SG_DAGNUM__IS_HARDWIRED_TEMPLATE(x) ( ((x) & SG_DAGNUM__BIT__AUDIT) || ((x) & SG_DAGNUM__BIT__HARDWIRED_TEMPLATE) )

// TODO think about types of dags.  we currently have two: tree
// and db.  this is encoded in the dagnum using a single bit
// for db.  if it's false, then it's a tree dag.  if we had a
// third type of dag, this would fail.  and if we only have
// two, I wish the bit were the other way around, since all dags
// except for VERSION_CONTROL are db dags.

// It would also really be nice to be able to group dags together 
// for push-pull purposes.  If a user does push --rev xyz, we want
// to push xyz from the version control dag, but also all the nodes
// from the version-control-related zing dags: stamps, comments, tags.

#define SG_DAGNUM__NONE              (                       0 )
#define SG_DAGNUM__VERSION_CONTROL   (                       1 )
#define SG_DAGNUM__WIKI              (  SG_DAGNUM__BIT__DB | 2 )
#define SG_DAGNUM__FORUM             (  SG_DAGNUM__BIT__DB | 3 )
#define SG_DAGNUM__WORK_ITEMS        (  SG_DAGNUM__BIT__DB | 4 )
#define SG_DAGNUM__VC_STAMPS         (  SG_DAGNUM__BIT__DB | 5 )
#define SG_DAGNUM__VC_COMMENTS       (  SG_DAGNUM__BIT__DB | 6 )
#define SG_DAGNUM__VC_TAGS           (  SG_DAGNUM__BIT__DB | 7 )
#define SG_DAGNUM__SIGNATURES        (  SG_DAGNUM__BIT__DB | 8 )
#define SG_DAGNUM__SUP               (  SG_DAGNUM__BIT__DB | 9 )
#define SG_DAGNUM__TESTING__DB       (  SG_DAGNUM__BIT__DB | 11 )
#define SG_DAGNUM__TESTING2__DB      (  SG_DAGNUM__BIT__DB | 12 )
#define SG_DAGNUM__BRANCHES          (  SG_DAGNUM__BIT__DB | 20 )
#define SG_DAGNUM__TESTING__NOTHING  (                       30 )
#define SG_DAGNUM__TESTING2__NOTHING (                       31 )

#define SG_DAGNUM__USERS             (  SG_DAGNUM__BIT__DB | 13 | SG_DAGNUM__BIT__ADMIN )

/* ---------------------------------------------------------------- */

#define SG_DAGNUM__ADMIN__DB__LICENSEKEYS

/* ---------------------------------------------------------------- */

/* TODO admin config db */

/* TODO test cases */

/* TODO builds and build results */

END_EXTERN_C;

#endif//H_SG_DAGNUM_TYPEDEFS_H

