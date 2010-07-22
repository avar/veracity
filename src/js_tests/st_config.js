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

load("vscript_test_lib.js");

//////////////////////////////////////////////////////////////////
// Some basic unit tests to test the sg.config.[...] global properties.
//////////////////////////////////////////////////////////////////

function stConfigTests()
{
    this.test_show_version = function()
    {
	// I can't really test what the correct values are.
	// All I can do is print all of the values that I 
	// know about at this time and let you eyeball it.

	print("sglib version fields are:");
	print("    [major " + sg.config.version.major + "]");
	print("    [minor " + sg.config.version.minor + "]");
	print("    [rev " + sg.config.version.rev + "]");
	print("    [buildnumber " + sg.config.version.buildnumber + "]");

	print("sglib version string is " + sg.config.version.string);

	print("sglib compiled with debug: " + sg.config.debug);
    }

}
