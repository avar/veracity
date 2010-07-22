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

function setUpLinks(repoName, hids) {

    if (hids.length > 1) {
        $('#recent').append($('<p>'));
        $('#recent').append("There are multiple heads in this repository");      
    }
    $.each(hids, function(i, hid) {
        $('#recent').append($('<p>'));
        var urlBase = "/repos/" + startRepo;
         $('#recent').append("head: ");
        var achange = $('<a>').attr({ 'title': 'view most recent changeset ' + hid, href: urlBase + "/changesets/" + hid })
                                     .text(hid).addClass("small_link");
        $('#recent').append(achange);
        var ul = $('<ul>');
        var liZip = $('<li>').append($('<a>').attr({ 'title': 'download zip file for ' + hid, href: urlBase + "/zip/" + hid })
                                            .text("zip").addClass("small_link"));
        var liBrowse = $('<li>').append($('<a>').attr({ 'title': 'browse changeset ' + hid, href: urlBase + "/changesets/" + hid + "/browsecs/" })
                                        .text("browse").addClass("small_link"));

        ul.append(liZip);
        ul.append(liBrowse);
        $('#recent').append(ul);


    });
   
}

function setRepoSelect(repos)
{
    var select = $('#reposelection').change(function() {
        document.location.href = $(this).val();
    });

    $.each(repos, function(i, v) {
        var url = "/repos/" + i + "/home/";
        var option = $('<option>').text(i).val(url).attr("id", i);

        select.append(option);
        if (i == startRepo) {
            setUpLinks(i, v);
            select.val(url);
        }

    });
}       
        
        
function sgKickoff() {
   
     getRepos(setRepoSelect);  

}