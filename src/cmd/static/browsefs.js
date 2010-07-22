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

var statuses = {};
var folderControl = {};

function _lastSeg(url) {
    var urlre = /.+\/(.+)\/?$/;

    var matches = urlre.exec(url);

    if (matches)
        return (matches[1]);
    else
        return (url);
}

function _normalizePath(url) {
    var parts = url.split('/');
    var checkagain = true;

    while (checkagain) {
        checkagain = false;

        for (var i = 1; i < parts.length; ++i) {
            if (parts[i] == "..") {
                parts.splice(i - 1, 2);
                checkagain = true;
                break;
            }
        }
    }
    
    var res = parts.join('/');
    
    return (res.replace(/\/\//, '/').replace(/\/$/, ''));
}

function status(path, id, stat) {
    this.repPath = path;
    this.hid = id;
    this.status = stat;
}

function _getStatuses() {

    $.ajax(
    {
        'url': '/local/status',
        dataType: 'json',

        error: function(xhr, textStatus, errorThrown) {
        },

        success: function(data) {
            statuses = {};

            for (var stat in data) {
                var files = data[stat];
                for (var i in files) {
                    var fn = files[i].path.replace(/^@/, '/local/fs');
                    statuses[fn] = new status(files[i].path, files[i].old_hid, stat);

                }

            }

        }

    });

}    

function _applyStatus(root, file) {
    
    var li = $('<li>').text(file);

    var fp = _normalizePath(root + "/" + file);

    if (statuses[fp]) {

        if (statuses[fp].status == "Modified") {

            var diffURL = "/local/repo/diff/" + statuses[fp].hid + "/" + statuses[fp].repPath;
            var a_diff = $('<a>').text("diff").attr({ href: "#", 'title': "diff against current working directory version" })
			                                        .addClass("download_link").click(function(e) {
			                                            showDiff(diffURL, $('<div>'), true);
			                                        });
            li.append(a_diff);

        }
        li.addClass(statuses[fp].status);
    }
    return li;
}

function _makeId(childurl) {
    var idre = /[^a-zA-Z0-9]/g;

    var url = childurl.replace(/\/[^\/]*\/?$/, '');

    var theid = url.replace(idre, "_");

    return (theid);
}


function loadDir(url, containerraw) {
    var container = $(containerraw);

    if (containerraw.id == 'ctr1') {
        container.attr("id", _makeId(_normalizePath(url)));
    }
   
    container.addClass('spinning');

    $.ajax(
    {
        'url': url,
        dataType: 'json',

        error: function(xhr, textStatus, errorThrown) {
            container.removeClass('spinning');
        },

        success: function(data) {
            container.removeClass('spinning');

            if (!data) {
                return;
            }

            container.empty();

            var h2 = $(document.createElement("h2"));
            var dn = _lastSeg(url);
            h2.text(dn);
            document.title = "veracity / browsing " + dn;
            container.append(h2);

            var ulFiles = $(document.createElement("ul")).addClass("files");
            var ulFolders = $(document.createElement("ul")).addClass("folders");
            data.files.sort(function(a, b) {
                return (a.name.toLowerCase().compare(b.name.toLowerCase()));
            });
            $.each(data.files,
			        function(index, entry) {
			            var li = $(document.createElement("li"));

			            if (entry.isdir) {
			                var curl = _normalizePath(url + "/" + entry.name);
			                var cid = _makeId(curl);
			                var a = folderControl.creatFolderDivLink(container, entry.name, cid, curl, loadDir, true);
			                
			                li.append(a);
			                ulFolders.append(li);
			            }
			            else {
			                li = _applyStatus(_normalizePath(url), entry.name);
			                ulFiles.append(li);
			            }
			        }
			 );

			container.append(ulFolders);
			container.append(ulFiles);
            $('div.dir').removeClass('selected');
            container.addClass('selected');

        }
    });

}


function sgKickoff() {
    
    var initurl = window.location.pathname;
    _getStatuses();
    folderControl = new browseFolderControl();
    loadDir(initurl, $('#ctr1')[0]);
}
