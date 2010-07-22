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

function _savedUser()
{
    return(_findCookie("sguserid"));
}

function _savedEmail() {
    return (_findCookie("sguseremail"));
}
function _saveUser(uid, email)
{
    var un = "sguserid";
    var em = "sguseremail";

    sgUserName = uid;
    sgUserEmail = email;

    _setCookie(un, '', -1);
    _setCookie(un, uid, 30);

    _setCookie(em, '', -1);
    _setCookie(em, email, 30);
}

function _hookUserSelector() {

    $('#userselection').val(sgUserName);
    if (!!_savedUser()) {
        $('#userselection').val(_savedUser());
    }


    $('#useremail').text(sgUserEmail);
    if (!!_savedEmail()) {

        $('#useremail').text(_savedEmail());
    }


    $('#userselection').change(
        function(e) {
            var email = $("#userselection option:selected").text();

            _saveUser($(this).val(), email);
            window.location.reload();
        }
    );
}

$(document).ready(_hookUserSelector);
