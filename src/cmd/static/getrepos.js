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


function displayRepos(data) {

    var url = window.location.pathname;

    $.each(data, function(index, value) {

        var tr = $('<tr>');
        var td = $('<td>');
        var tdtags = $('<td>');
        var tdzip = $('<td>');


        var a = $('<a>').attr({ 'title': 'view repository history', href: url + "/" + index + "/history" })
                                  .text(index);

        td.append(a);
        tr.append(td);
        var atags = $('<a>').attr({ 'title': 'view tags', href: url + "/" + index + "/tags/" })
                                        .text("tags").addClass("small_link");

        tdtags.append(atags);
        var hids = value;

        if (hids.length > 1) {

            var azip = $('<a>').attr({ 'title': 'view repo heads', href: "#" }).text("heads");
            tdzip.append(azip);
            azip.click(function() {

                var table = $('<table>');

                $.each(hids, function(i, v) {

                    var tr2 = $('<tr>');
                    var td2 = $('<td>');
                    var tdzip2 = $('<td>');

                    var cslink = makePrettyRevIdLink(v, true, null, null, url + "/" + index);
                    td2.append(cslink);
                    var azip = $('<a>').attr({ 'title': 'download zip file for ' + v, href: url + "/" + index + "/zip/" + v })
                                            .text("zip").addClass("small_link");

                    var achange = $('<a>').attr({ 'title': 'view most recent changeset ' + v, href: url + "/" + index + "/changesets/" + v })
                                         .text("changeset").addClass("small_link");

                    var abrowse = $('<a>').attr({ 'title': 'browse changeset ' + v, href: url + "/" + index + "/changesets/" + v + "/browsecs/" })
                                        .text("browse").addClass("small_link");


                   
                    tdzip2.append(achange);
                    tdzip2.append(abrowse);
                    tr2.append(td2);
                    tr2.append(tdzip2);
                    table.append(tr2);

                });
                var pop = new modalPopup();

                $('#maincontent').prepend(pop.create("heads", table));
                pop.show(null, null, tdzip.position());

            });

        }

        else {

            var azip = $('<a>').attr({ 'title': 'download zip file for ' + hids[0], href: url + "/" + index + "/zip/" + hids[0] })
                    .text("zip").addClass("small_link");

            var achange = $('<a>').attr({ 'title': 'view most recent changeset ' + hids[0], href: url + "/" + index + "/changesets/" + hids[0] })
                    .text("changeset").addClass("small_link");

            var abrowse = $('<a>').attr({ 'title': 'browse most recent changeset' + hids[0], href: url + "/" + index + "/changesets/" + hids[0] + "/browsecs/" })
                    .text("browse").addClass("small_link");

            tdzip.append(azip);
            tdzip.append(achange);
            tdzip.append(abrowse);           

        }
        tr.append(tdzip);
        tr.append(tdtags);
        $('#tablerepos').append(tr);
    });

    $('table.datatable tr').hover(
			            function() {
			                $(this).css("background-color", "#E4F0CE");
			            },
                        function() {
                            $(this).css("background-color", "white");
                        });


}

function sgKickoff() {

    getRepos(displayRepos);                          
       
}