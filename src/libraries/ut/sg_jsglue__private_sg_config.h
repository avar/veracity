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
 * @file sg_jsglue__private_sg_config.h
 *
 * @details Set configuration-related properties in "sg.config.{...}".
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_JSGLUE__PRIVATE_SG_CONFIG_H
#define H_SG_JSGLUE__PRIVATE_SG_CONFIG_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Set properties on the global "sg" symbol for various
 * compile-time configuration options and values.  These
 * will allow any consumer of the JS API to ask certain
 * questions about sglib.
 *
 * Most of these values should be considered constants
 * (and some have the READONLY property bit set).
 *
 * Others might be overwritten by the test harness
 * and/or the build machine.  (But just because the
 * test harness changes a value doesn't mean that sglib
 * can support it (without a recompile).)
 */
static void sg_jsglue__install_sg_config(SG_context * pCtx,
										 JSContext * cx,
										 JSObject * pjsobj_sg)
{
	JSObject * pjsobj_config = NULL;
	JSObject * pjsobj_version = NULL;

	SG_JS_BOOL_CHECK(  pjsobj_config  = JS_DefineObject(cx, pjsobj_sg,     "config",  NULL, NULL, 0)  );	// sg.config.[...]
	SG_JS_BOOL_CHECK(  pjsobj_version = JS_DefineObject(cx, pjsobj_config, "version", NULL, NULL, 0)  );	// sg.config.version.[...]

	// Define CONSTANTS equivalent to:
	//     var sg.config.version.major       = "1";
	//     var sg.config.version.minor       = "0";
	//     var sg.config.version.rev         = "0";
	//     var sg.config.version.buildnumber = "1000";
	//     var sg.config.version.string      = "1.0.0.1000 (Debug)";

	SG_ERR_CHECK(  sg_jsglue__define_property__sz(pCtx, cx, pjsobj_version, "major",        MAJORVERSION)  );
	SG_ERR_CHECK(  sg_jsglue__define_property__sz(pCtx, cx, pjsobj_version, "minor",        MINORVERSION)  );
	SG_ERR_CHECK(  sg_jsglue__define_property__sz(pCtx, cx, pjsobj_version, "rev",          REVVERSION  )  );
	SG_ERR_CHECK(  sg_jsglue__define_property__sz(pCtx, cx, pjsobj_version, "buildnumber",  BUILDNUMBER )  );
	SG_ERR_CHECK(  sg_jsglue__define_property__sz(pCtx, cx, pjsobj_version, "string",      (MAJORVERSION "." MINORVERSION "." REVVERSION "." BUILDNUMBER DEBUGSTRING))  );

	// Define CONSTANTS equivalent to:
	//     var sg.config.debug = true;

#if defined(DEBUG)
	SG_ERR_CHECK(  sg_jsglue__define_property__bool(pCtx, cx, pjsobj_config, "debug",  SG_TRUE )  );
#else
	SG_ERR_CHECK(  sg_jsglue__define_property__bool(pCtx, cx, pjsobj_config, "debug",  SG_FALSE )  );
#endif

	// TODO 4/2/10 See if we want to put in some OS and/or hardware info.  Mac vs Linux vs Windows,
	// TODO 4/2/10 have-symlinks, have-xattrs, have-attrbits, 32 vs 64 bits, etc.

fail:
	return;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_JSGLUE__PRIVATE_SG_CONFIG_H
