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

var baseBrowse = findBaseURL("browsecs");
var baseCS = findBaseURL("changesets");

var hideIndex = 0;
var folderControl = {};


function displayBrowse(data, container) {

    container.attr({ id: data.path.rtrim("/") });
    $('#csid').html(makePrettyRevIdLink(data.changeset_id, false, null, null, findBaseURL("changesets")));
    $('#csid').prepend("Changeset ID: ");

    document.title = "veracity / browse changeset " + data.name;
    if (data.type == "File") {

        var parentUrl = baseBrowse + "/browsecs/" + getParentPath(data.path);
        loadBrowse(parentUrl, $('#ctr1')[0]);
        displayFile(data.hid, null, data.name);

    }
    else {
        var h2 = $('<h2>');
        container.append(h2);
        var ulFiles = $('<ul>').addClass("files");
        var ulFolders = $('<ul>').addClass("folders"); ;

        h2.text(data.name);
        var a = $('<a>');

        data.contents.sort(function(a, b) {
            return (b.name.toLowerCase().compare(a.name.toLowerCase()));
        });
        $.each(data.contents, function(index, entry) {
            var isDir = (entry.type == "Directory");
            var li = $('<li>');
            var curl = baseBrowse + "/browsecs/" + entry.gid;

            if (isDir) {

                a = folderControl.creatFolderDivLink(container, entry.name, entry.path.rtrim("/"), curl, loadBrowse, false);
                li.prepend(a);
                ulFolders.prepend(li);
            }

            else {
                a = folderControl.creatFileDivLink(container, entry.hid, entry.name);

                var downloadURL = baseCS + "/blobs-download/" + entry.hid + "/" + lastURLSeg(entry.path);

                var a_download = $('<a>').html("&#8650;").attr({ href: downloadURL, 'title': "download file" })
			                                        .addClass("download_link");

                li.append(a_download);

                if (isLocal()) {
                    var diffURL = baseCS + "/diff/" + entry.hid + "/" + entry.path;

                    var a_diff = $('<a>').text("diff").attr({ href: "#", 'title': "diff against current working directory version" })
			                                        .addClass("download_link").click(function(e) {
			                                            e.preventDefault();
			                                            e.stopPropagation();
			                                            showDiff(diffURL, diff_div, true);

			                                        });
                    li.append(a_diff);
                }
                li.prepend(a);
                ulFiles.prepend(li);
            }

        });

        if (data.path.rtrim("/") != "@") {

            var li = $('<li>');
            var parent = getParentPath(data.path);
            var curl = baseBrowse + "/browsecs/" + parent;
            a = folderControl.creatFolderDivLink(container, "..", parent, curl);
            li.append(a);
            ulFolders.prepend(li);
        }

        else {
            container.attr({ id: "@" });
        }

        container.append(ulFolders);
        container.append(ulFiles);
        $('div.dir').removeClass('selected');
        container.addClass('selected');
    }

}
function loadBrowse(url, containerraw) {

    var container = $(containerraw);
    
    //for modal  
    var bg = $('<div>').attr({ id: "backgroundPopup" });
    container.prepend(bg);

    var diff_div = $('<div>').addClass("cs_info");
    $('#maincontent').append(diff_div);
    container.addClass('spinning');

    $.ajax({
        dataType: 'json',
        url: url,
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            container.removeClass('spinning');
            reportError(url, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {

            container.removeClass('spinning');

            displayBrowse(data, container);

        }
    });

}

function sgKickoff() {

    var initurl = window.location.pathname;
    folderControl = new browseFolderControl();
    loadBrowse(initurl, $('#ctr1')[0]);
}
