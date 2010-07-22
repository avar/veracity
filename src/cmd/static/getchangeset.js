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

var changesetID;
var base = findBaseURL("changesets");
var stamps = [];


function setTimeStamps(ts, div) {
    var users = [];
    var dates = [];
    $.each(ts, function(i, v) {

        var d = new Date(parseInt(v.when));
        var who = v.email || v.who;
        if (div) {
            var csig = $('<span>').text(who);
            csig.append($('<br/>'));
            csig.append(shortDateTime(d));
            div.append(csig);
            div.addClass("sig");
        }
        else {
            $('#who').append(who);
            $('#when').append(shortDateTime(d));
        }
    });   
   
  
}

function setComments(comments) {
    var cs = [];

    if (comments) {
        $('#comment').empty();
        comments.sort(
		       function(a, b) {
		           return (b.history[0].audits[0].when - a.history[0].audits[0].when);
		       }
		    );
		       $.each(comments, function(i, v) {
		           var cdiv = $('<div>').addClass("greenblock");
		           cdiv.text(v.text);
		           cdiv.append($('<br/>'));
		           var span = $('<span>');
		           setTimeStamps(v.history[0].audits, span);
		           $('#comment').append(cdiv);
		           $('#comment').append(span);
		           $('#comment').append($('<p>'));

		       });

    }
}



function showChangedFiles(diff, div) {
    var ul = $('<ul>').addClass("cslist");

    $.each(diff,
			  function(index, value) {

			      value.sort(function(a, b) {
			          return (b.path.toLowerCase().compare(a.path.toLowerCase()));
			      });
			      $.each(value, function(ii, vv) {
			          var isDirectory = (vv.type == "Directory");
			          var li = $('<li>');
			          var sp = $('<span>');
			          var toggle = $('<span>');
			          sp.text(index + " ");
			          var plus = $('<a>').text("+").attr({ href: "#" }).addClass("plus");
			          var info = $('<div>').addClass("cs_info");

			          plus.toggle(function() {

			              plus.text("-");
			              if (index == "Modified") {
			                  var urlPrev = base + "/diff/" + vv.old_hid + "/" + vv.hid + "/" + lastURLSeg(vv.path);
			                  //only show the local option if browsing local
			                  if (isLocal()) {
			                      var urlLocal = base + "/diff/" + vv.hid + "/" + vv.path;
			                      var diff_a = $('<a>').text("(show working directory diff)").attr({ href: "#" })
			                        .toggle(
			                          function() {
			                              diff_a.text("(show parent diff)");
			                              showDiff(urlLocal, info);
			                          },
			                          function() {
			                              diff_a.text("(show working directory diff)");
			                              showDiff(urlPrev, info);
			                          });
			                      info.html(diff_a);
			                  }
			                  showDiff(urlPrev, info);

			              }
			              info.css({ "padding-top": "5px", "padding-bottom": "5px" });
			              info.show();
			          },
                     function() {
                         plus.text("+");
                         info.hide();
                     });

			          toggle.append(plus);
			          if (index != "Added" && index != "Removed") {
			              li.append(toggle);
			              li.addClass("toggle");

			          }

			          li.append(sp);

			          var a = $('<a>').text(vv.path);

			          if (isDirectory) {
			              a.attr({ href: window.location.pathname + "/browsecs/" + vv.gid, 'title': "display folder" })

			          }
			          else {
			              a.attr({ href: "#", 'title': "display file" })
			                                            .click(function() {
			                                                displayFile(vv.hid, vv.path.ltrim("@/"));
			                                            });
			          }

			          li.append(a);

			          if (vv.from) {
			              info.append(" moved from " + vv.from).hide();

			          }
			          if (vv.oldname) {
			              info.append(" original name " + vv.oldname).hide();

			          }

			          if (isDirectory == false) {
			              var downloadURL = base + "/blobs-download/" + vv.hid + "/" + lastURLSeg(vv.path);
			              var img = $('<img>').attr("src", "/static/css/images/downloadgreen2.png");
			              var a_download = $('<a>').html(img).attr({ href: downloadURL, 'title': "download file" })
			                                        .addClass("download_link");
			              li.append(a_download);
			          }


			          ul.prepend(li);
			          li.append(info);
			      });
			  }
			 );
    if (div) {     
        div.append(ul);
    }
    else {
        $('#changes').append(ul);
    }

}


function setChangedFiles(data, parents) {
    $('#browse').attr("href", window.location.pathname + "/browsecs/");

    $.each(parents, function(i, v) {

        if (data.description.parent_count == 1) {
            showChangedFiles(data[i]);
        }
        else {
            var ul = $('<ul>');
            var li = $('<li>').text(i);

            var div = $('<div>').attr({ id: i }).hide();
            div.append($('<br/>'));
            showChangedFiles(data[i], div);
            div.append($('<br/>'));
            var a = $('<a>').attr({ 'title': 'show changes', href: "#" })
                    .text("+").addClass("plus").toggle(function() {
                        a.text("-");
                        div.show();
                    },

                 function() {
                     a.text("+");
                     div.hide();
                 });

            li.prepend(a);
            li.append(div);
            ul.append(li);
        }
        $('#changes').append(ul);
    });

}

function setParents(parents) {
    
    if (parents) {

        $.each(parents, function(i, v) {
            $('#parents').append(makePrettyRevIdLink(i, true, null, null, base));
            $('#parents').append($('<br/>'));
            $('#parent_nav').append(makePrettyRevIdLink(i, false, " <", null, base));
            var div = $('<div>').addClass("greenblock");
            var span = $('<span>');
            setTimeStamps(v.audits, span);

            if (v.comments && v.comments.length > 0) {

                v.comments.sort(
		                    function(a, b) {
		                        return (a.history[0].audits[0].when - b.history[0].audits[0].when);
		                    });
		                    div.append(v.comments[0].text);
		                    $('#parents').append(div);
            }          
           
            $('#parents').append(span);
            $('#parents').append($('<p>'));
        });
    }

}

function setChildren(children) {
    if (children && children.length > 0) {
     
        $.each(children, function(i, v) {

            $('#child_nav').append(makePrettyRevIdLink(v, false, null, ">", base));

        });
    }
    else
        $('#children').hide();

}

function deleteStamp(stamp) {

    var url = window.location.pathname + "/stamps/" + stamp;
    $.ajax(
    {
        type: "DELETE",
        url: url,
        data: stamp,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },
        success: function(data, status, xhr) {
            var stamps = [];
            $.each(data, function(i, v) {
                stamps.push(v.stamp);
            });
            setStamps(stamps);
         
        },
        error: function(xhr, textStatus, errorThrown) {         
            reportError(null, xhr, textStatus, errorThrown);
        }

    });

}

function setStamps(stamps) {
    $('#stamps span').remove();
    if (stamps != null && stamps.length > 0) {

        var div = $('#stamps');
        div.addClass("greenblock");     
      
        $.each(stamps, function(i, v) {
            var a = $('<a>').text(v).attr({ "title": "view items with this stamp", href: base + "/history?stamp=" + v });
            var del = $('<a>').text("x").attr({ "title": "delete this stamp", href: "#" });

            var cont = $('<span>').addClass("cont");
            var span = $('<span>').append(a).addClass("stamp");
            cont.append(span);
            cont.append(del);

            div.append(cont);
            del.click(function(e) {
                e.preventDefault;
                e.stopPropegation;
                confirmDialog("Delete " + v + "?", del, deleteStamp, v);

            });

            if ((i > 1) && (i % 5 == 0)) {
                div.append($('<p>'));
            }
        });
    }
    
}

function getAllStamps() {

    var url = base + "/stamps/";
    
    var allstamps = [];
    $.ajax({
        url: url,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {

            if ((status != "success") || (!data)) {
                return;
            }
           
            $.each(data, function(i, v) {

                allstamps.push(v.stamp);
            });
            $("#inputstamp").autocomplete({
                source: allstamps,
                minLength: 3
            });
        }

    });

}

function postComment() {

    var url = window.location.pathname + "/comments/";

    var text = $('#inputcomment').val();

    if (!text) {

        reportError("comment can not be empty", null, "error");

        return;
    }
 
    $('#maincontent').addClass('spinning');
    
    $.ajax(
    {
        type: "POST",
        url: url,
        data: $('#inputcomment').val(),
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },
        success: function(data, status, xhr) {
            setComments(data);
            $('#maincontent').removeClass('spinning');
        },
        error: function(xhr, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        }

    });

    $('#inputcomment').val('');
}

function getStamps() {

    $('#maincontent').css("height", "300px");

    var str = "";

    var url = _base + "/stamps/";

    stamps = [];
    $.ajax({
        url: url,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {
           
            $('#maincontent').css("height", "");
            if ((status != "success") || (!data)) {
                return;
            }
            $.each(data, function(i, v) {

                stamps.push(v.stamp);
            });           
        }

    });

}

function postStamp() {

    var url = window.location.pathname + "/stamps/";
    var text = $('#inputstamp').val();

    if (!text) {
             
        reportError("stamp can not be empty", null, "error");

        return;
    }

    $.ajax(
    {
        type: "POST",
        url: url,
        data: text,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },
        success: function(data, status, xhr) {
            var stamps = [];
            $.each(data, function(i, v) {
                stamps.push(v.stamp);
            });
            setStamps(stamps);
            $('#inputstamp').val("");
            getAllStamps();
        },
        error: function(xhr, textStatus, errorThrown) {
            $('#comments').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        }

    });

    $('#inputcomment').val('');
}

function moveTagToThis(oldCsid) {
  
    var text = $('#inputtag').val();
    var url = base + "/changesets/" + oldCsid + "/tags/" + text;
    deleteTag(text, url, postTag);
    
}

function deleteTag(tag, delurl, onSuccess) {

    var url = delurl || window.location.pathname + "/tags/" + tag;
    $.ajax(
    {
        type: "DELETE",
        url: url,
        data: tag,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },
        success: function(data, status, xhr) {
            if (onSuccess) {
                onSuccess();
            }
            else {
                var tags = [];
                $.each(data, function(i, v) {
                    tags.push(v.tag);
                });
                setTags(tags);
            }

        },
        error: function(xhr, textStatus, errorThrown) {
            reportError(null, xhr, textStatus, errorThrown);
        }

    });

}

function setTags(tags) {
    $('#tags span').remove();
    if (tags != null && tags.length > 0) {
      
        var div = $('#tags');
        div.addClass("greenblock");
        $.each(tags, function(i, v) {

            var span = $('<span>').append(v).addClass("tag");
            var del = $('<a>').text("x").attr({ "title": "delete this tag", href: "#" });
            
            var cont = $('<span>').addClass("cont");
            cont.append(span);
            cont.append(del);
            div.append(cont);
            del.click(function(e) {
                e.preventDefault;
                e.stopPropegation;

                confirmDialog("Delete " + v + "?", del, deleteTag, v);
            });

            if ((i > 1) && (i % 5 == 0)) {
                div.append($('<p>'));
            }
        });

    }
}


function postTag() {

    var url = window.location.pathname + "/tags/";

    var text = $('#inputtag').val();
    
    if (!text) {

        reportError("tag can not be empty", null, "error");
        return;
    }

    $.ajax(
    {
        type: "POST",
        url: url,
        data: text,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },
        success: function(data, status, xhr) {
            if (data.duplicate) {
                var div = $('<div>').text("The tag exists on this changesent, move tag?");
                div.append($('<br/><br/>'));
                var a = ($('<a>')).attr({ href: base + "/changesets/" + data.changeset.changeset_id, "title": "view changeset", "target": "_blank" })
                    .text(data.changeset.changeset_id);
                div.append(a);
                div.append($('<br/>'));
                setTimeStamps(data.changeset.audits, div);
                div.append($('<br/>'));
                div.append($('<br/>'));
                if (data.changeset.comments && data.changeset.comments.length > 0) {

                    data.changeset.comments.sort(
		                    function(a, b) {
		                        return (a.history[0].audits[0].when - b.history[0].audits[0].when);
		                    });

                    div.append(data.changeset.comments[0].text);
                }

                confirmDialog(div, $('#inputtag'), moveTagToThis, data.changeset.changeset_id);
            }
            else {

                var tags = [];
                $.each(data, function(i, v) {
                    tags.push(v.tag);
                });
                setTags(tags);
                $('#inputtag').val("");
            }
        },
        error: function(xhr, textStatus, errorThrown) {
            reportError(null, xhr, textStatus, errorThrown);
        }

    });

    $('#inputcomment').val('');
}



function getChangeSet() {

    var listUrl = window.location.pathname;

    $('#maincontent').addClass('spinning');

    $.ajax(
    {
        url: listUrl,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {

            changesetID = data.description.changeset_id;
            $('#maincontent').removeClass('spinning');
            $('#revid').text(data.description.changeset_id);
            $('#revid_short').text(data.description.changeset_id.slice(0, 10));
            $('#current_nav').text(data.description.changeset_id.slice(0, 10));

            $('#zip_repo').attr({ 'title': 'download zip of repo at this version', href: base + "/zip/" + data.description.changeset_id });

            setTimeStamps(data.description.audits);
            setComments(data.description.comments);
            setParents(data.description.parents);            
            setChangedFiles(data, data.description.parents);
            setChildren(data.description.children);
            setTags(data.description.tags);
            setStamps(data.description.stamps);

        }
    });   
}

function sgKickoff() {
    getChangeSet();
    getAllStamps();
   
}
