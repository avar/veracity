
OBJS = jsapi.obj jsarena.obj jsarray.obj jsatom.obj jsbool.obj jscntxt.obj jsdate.obj jsdbgapi.obj jsdhash.obj jsdtoa.obj jsemit.obj jsexn.obj jsfile.obj jsfun.obj jsgc.obj jshash.obj jsinterp.obj jsinvoke.obj jsiter.obj jslock.obj jslog2.obj jslong.obj jsmath.obj jsnum.obj jsobj.obj jsopcode.obj jsparse.obj jsprf.obj jsregexp.obj jsscan.obj jsscope.obj jsscript.obj jsstr.obj jsutil.obj jsxdrapi.obj jsxml.obj prmjtime.obj

.c.obj:
	cl -c -I. -DXP_WIN -DEXPORT_JS_API -nologo -MD -Od -D_X86_=1 -D_CRT_SECURE_NO_DEPRECATE $?

sgbrings_js.lib: jsautokw.h $(OBJS)
	lib /out:sgbrings_js.lib $(OBJS)

jskwgen.exe: jskwgen.c
	cl -I. -DXP_WIN -DEXPORT_JS_API -nologo -MD -Od -D_X86_=1 -D_CRT_SECURE_NO_DEPRECATE jskwgen.c

jsautokw.h: jskwgen.exe jskeyword.tbl
	jskwgen.exe jsautokw.h jskeyword.tbl



