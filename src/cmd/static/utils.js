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

function _findCookie(cname)
{
    var un = cname + "=";
    var v = false;

    if (document.cookie.length > 0)
    {
        var cookies = document.cookie.split(';');

        for ( var i = 0; i < cookies.length; ++i )
        {
            var cstr = jQuery.trim(cookies[i]);

            if (cstr.substring(0, un.length) == un)
            {
                // don't just return; in corner cases, multiple versions
                // of the cookie may exist. we want the last one.
                v = decodeURIComponent(cstr.substring(un.length));
            }
        }
    }

    return(v);
}

function _setCookie(cname, cval, exp)
{
    var exdate=new Date();
    exdate.setDate(exdate.getDate() + exp);

    var cstr = cname + "=" + encodeURIComponent(cval) +
        "; path=/" +
        "; expires="+exdate.toUTCString();

    document.cookie = cstr;
}


function _debugOut(msg) {

    try {
    
        if ((!!window.console) && (!!console.log)) {
            console.log(msg);
        }
        else if ((!! window.opera) && (!! opera.postError))
        {
            opera.postError(msg);
        }
    }
    catch (e)
    {
    }
}

function stretch(t) {
    if (t < 10) {
        t = "0" + t;
    }

    return (t);
}


var actMonths = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

function shortDateTime(d) {

    var result = actMonths[d.getMonth()] + " " + d.getDate() + " at " +
	stretch(d.getHours()) + ":" + stretch(d.getMinutes());

    return (result);
}

function _shortWho(longwho)
{
    var rewho = /^(.+?)[ \t]*<.+>[ \t]*$/;

    var matches = longwho.match(rewho);

    if (matches)
	return(matches[1]);

    return(longwho);
}

function isLocal() {
    return window.location.pathname.indexOf("/local/") > 0;

}

//this is a list of files that will show up one the changeset page when the file is clicked. This is
//by no means a complete list. eventually we should check the repo/local setting for theis information
var mergeFiles = ["txt", "csv", "xml", "bat", "dat", "asp", "cer", "csr", "css", "htm", "js", "jsp",
"php", "rss", "xhtml", "config", "aspx", "ascx", "asmx", "c", "cpp", "java", "pl", "h", "csproj",
"cs", "cc", "class", "ctl", "dob", "dox", "frm", "frx", "hh", "hpp", "jav", "java", "json", "licx",
"mak", "mf", "p", "pl", "py", "rss", "sh", "suo", "vbproj", "vbp", "vbx", "vbz", "vbg", "vcproj", "xsd"];

function isMergeable(file) {

    var segs = file.split(".");
    if (segs.length < 2)
        return false;

    return (mergeFiles.indexOf(segs[segs.length - 1]) != -1);

}

function findBaseURL(lastSeg) {

    var url = window.location.pathname;
    if (url.indexOf(lastSeg) > 0) {
        url = url.substring(0, url.indexOf(lastSeg));
    }
  
    return url.rtrim("/");
}

function makePrettyRevIdLink(id, expandable, prepend, append, urlBase) {

    var base = urlBase || "/repos/" + encodeURI(startRepo);
    var myURL = urlBase + "/changesets/" + id;

    var span = $('<span>');

    var csa = $('<a>').text((prepend ? prepend : "") + id.slice(0, 10) + (append ? append : ""))
                           .attr({ href: myURL , 'title': "view changeset " + id});

    span.append(csa);

    if (expandable) {      
        var a = $('<a>').text("...").attr({ href: "#" });

        span.append(a);
        
        a.click(function(e) {
            e.preventDefault;
           
            var info = $('<span>').text(id);			          
            
            var pop = new modalPopup("#none");

            $('#maincontent').prepend(pop.create("", info));

            pop.show(null, null, a.position());
        });
    }
    return span;

}

function makeId(childurl) {
    var idre = /[^a-zA-Z0-9]/g;

    var url = childurl.replace(/\/[^\/]*\/?$/, '');

    var theid = url.replace(idre, "_");

    return (theid);
}

function lastURLSeg(url) {

    var p = url || window.location.pathname;
    p = p.rtrim("/");

    var ar = p.split('/');

    return (ar[ar.length-1]);

}

function getParentPath(path) {

    var parent = path.rtrim("/");

    var index = parent.lastIndexOf("/");

    if (index < 0) {

        return "@";
    }

    parent = parent.slice(0, index);

    return parent;

}

function cwd() {
    var p = window.location.pathname;
    if(p[p.length-1]=='/')
        return p;
    else
        return p+'/';
}

function isScrollBottom() {
    var documentHeight = $(document).height();
    var windowHeight = $(window).height() || window.innerHeight;
    var scrollP = windowHeight + $(window).scrollTop();
 
    return (scrollP >= documentHeight);
}


function vButton(text) {

    var span = $('<span>').addClass("button").text(text);
   
    return span;

}

//message can be object (span, div etc)
//optional posObject used to position the dialog
//optional onOKfunction is called when the user clicks yes
//optional args for the onOKfunction
function confirmDialog(message, posObject, onOKfunction, args) {

    this.ok = false;  
    
    var div = $('<div>').append(message);
    var buttonOK = new vButton("yes");
    var buttonCancel = new vButton("no");
    var span = $('<span>').addClass("dialogButtons");
    
    buttonOK.click(function() {
        this.ok = true;
        pop.close();
        onOKfunction(args);

    });
  
    buttonCancel.click(function() {

        this.ok = false;
        pop.close();

    });
   
    div.append($('<br/><br/>'));
    span.append(buttonOK);
    span.append(buttonCancel);
    div.append(span);
    var pop = new modalPopup("none");

    $('#maincontent').prepend(pop.create("confirm action", div));
    pop.show(null, null, posObject.position());
   
}

function browseFolderControl() {

    hideIndex = 0;

    this.creatFolderDivLink = function(container, name, cid, url, browseFunction, fromFS) {

        var a = $('<a>').text(name);
        a.attr({ href: "#", 'title': "view folder" });
        a.click(function(e) {
            container.find('li').removeClass("selected");
            $(this).parent().addClass("selected");

            e.preventDefault();
            e.stopPropagation();
            var windowWidth = $(window).width() || window.innerWidth;
            var newCtr = $(document.getElementById(cid));

            if (newCtr.length == 0) {
                newCtr = $('<div>');
                newCtr.attr({ id: cid });
                newCtr.addClass('dir');

                if ((container.position().left + container.width() * 3) > windowWidth) {
                    $('div.dir').eq(hideIndex).hide();
                    hideIndex++;
                }
                container.after(newCtr);
                
                if (!fromFS) {
                    browseFunction(url, newCtr);
                }
            }
       
            if (fromFS) {
                browseFunction(url, newCtr);
            }

            newCtr.nextAll().remove();

            if (hideIndex > 0) {

                if ((newCtr.position().left + newCtr.width() * 3) < windowWidth) {
                    $('div.dir').eq(hideIndex - 1).show();
                    hideIndex--;
                }

            }

        });

        return a;
    }
    
    this.creatFileDivLink = function(container, hid, name) {

        var a = $('<a>').text(name);
        a.attr({ href: "#", 'title': "view file" });
        a.click(function(e) {

            e.preventDefault();
            e.stopPropagation();
            displayFile(hid, null, name);

        });
        return a;
    }
}

function modalPopup(backID) {

    var popupStatus = 0;
    this.backID = backID || "#backgroundPopup";

    this.create = function(title, content, menu) {

        var div = $('<div>').attr({ id: "popup" });
        var x = $('<a>').attr({ href: "#", id: "popupClose" }).text("x").click(function() { closePopup(); });

        var h1 = $('<h1>').attr({ id: "popupTitle" }).text(title);
        if (menu) {
            h1.append(menu);
        }
        var content_div = $('<div>').attr({ id: "content" });
        div.append(h1);
        div.append(x);
        content_div.html(content);
        div.append(content_div);

        //Click out event
        $(this.backID).click(function() {
            closePopup();
        });
        //Press Escape event
        $(document).keyup(function(e) {
            if (e.keyCode == '27') {
                e.preventDefault();
                closePopup();
            }
        });

        return div;

    }

    this.close = function() {
   
        closePopup();
    }
    this.show = function(height, width, position) {
        if (height) {
            $("#popup").css({
                "height": height
            });
        }
        if (width) {
            $("#popup").css({
                "width": width
            });
        }

      
        if (!position) {
            center();
        }
        else {
            var top = position.top;
            var left = position.left;
            $("#popup").css({
                "position": "absolute",
                "top": top,
                "left": left
            });
        }

        //loads popup only if it is disabled
        if (popupStatus == 0) {

            if (this.backID) {
                $(this.backID).css({
                    "opacity": "0.7"
                });
                $(this.backID).fadeIn("slow");
            }
            $("#popup").fadeIn("slow");
            popupStatus = 1;
        }
    }

    //centering popup
     function center() {
         //request data for centering
        var windowHeight = $(window).height() || window.innerHeight;
        var windowWidth = $(window).width() || window.innerWidth;
        
        var popupHeight = $("#popup").height();
        var popupWidth = $("#popup").width();

        //if the popup is larger than the screen set the
        //top to a constant else center it on the screen
        var left = 5;
        var top = 30;
      
        if (popupWidth < windowWidth) {
           left = (windowWidth / 2) - (popupWidth / 2);
        }
        if (popupHeight < windowHeight) {
            top = (windowHeight / 2) - (popupHeight / 2);
        }
        $("#popup").css({
            "position": "absolute",
            "top": top,
            "left": left
        });
      
    }
    function closePopup() {
      
        if (popupStatus == 1) {

            $(backID).fadeOut("slow");
            $("#popup").fadeOut("slow");
            popupStatus = 0;
        }
    }
}

function getQueryStringVals() {
    var vars = [], hash;
    var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    
    for (var i = 0; i < hashes.length; i++) {
        hash = hashes[i].split('=');        
        vars.push(hash[0]);
        vars[hash[0]] = hash[1];      
    }
    return vars;
}

function getQueryStringVal(name) {
    var queryhash = getQueryStringVals();
    return queryhash[name];
}

String.prototype.trim = function(chars) {

    var str = this.rtrim(chars);
    str = this.ltrim(chars);
    return str;

}
String.prototype.ltrim = function(chars) {
    chars = chars || "\\s";
    return this.replace(new RegExp("^[" + chars + "]+", "g"), "");
}

String.prototype.rtrim = function(chars) {
    chars = chars || "\\s";
    return this.replace(new RegExp("[" + chars + "]+$", "g"), "");
}

String.prototype.compare = function(b) {
    var a = this + '';
    return (a === b) ? 0 : (a > b) ? 1 : -1;
}


function reportError(message, xhr, textStatus, errorThrown) {
    var msg = message || "";

    if (!!errorThrown)
        msg += "\r\n" + errorThrown;

    if (xhr && !!xhr.responseText)
        msg += "\r\n" + xhr.responseText;

    var div = $('<pre>').text(msg);

    var pop = new modalPopup("none");

    $('body').prepend(pop.create(textStatus, div));
    pop.show();

}


//For IE
function parseJSONforIE(xml) {

    var xmlDoc = new ActiveXObject("Microsoft.XMLDOM");
    xmlDoc.loadXML(xml);
    xml = xmlDoc;

    return xml;
}

//IE 6 hack
if (!Array.prototype.indexOf) {
    Array.prototype.indexOf = function(obj, fromIndex) {
        if (fromIndex == null) {
            fromIndex = 0;
        } else if (fromIndex < 0) {
            fromIndex = Math.max(0, this.length + fromIndex);
        }
        for (var i = fromIndex, j = this.length; i < j; i++) {
            if (this[i] === obj)
                return i;
        }
        return -1;
    };
}

function sleep(delay) {

    var start = new Date().getTime();
    while (new Date().getTime() < start + delay);
}


function fitToContent(text, maxHeight) {
    var pn = $(text.parent());
    var l = text.position().left;
    var pw = pn.innerWidth();

    text.css('width', (pw - l - 50) + "px");

        var adjustedHeight = text.height();
        var relative_error = parseInt(text.attr('relative_error'));
        if (!maxHeight || maxHeight > adjustedHeight) {
                adjustedHeight = Math.max(text[0].scrollHeight, adjustedHeight);
                if (maxHeight)
                        adjustedHeight = Math.min(maxHeight, adjustedHeight);
                if ((adjustedHeight - relative_error) > text.height()) {
                        text.css('height', (adjustedHeight - relative_error) + "px");
                        // chrome fix
                        if (text[0].scrollHeight != adjustedHeight) {
                                var relative = text[0].scrollHeight - adjustedHeight;
                                if (relative_error != relative) {
                                        text.attr('relative_error', relative + relative_error);
                                }
                        }
                }
        }
}

function autoResizeText(ta, maxHeight) {
        var resize = function() {
            fitToContent($(this), maxHeight);
        };
        $(ta).attr('relative_error', 0);
        $(ta).each(resize);
        $(ta).keyup(resize).keydown(resize);
}


//AJAX common calls 
function showDiff(url, container, showAsPopup) {

    container.find('pre').remove();

    $.ajax({
        url: url,
        cache: false,
        success: function(data, status, xhr) {

            $('#maincontent').removeClass('spinning');
            var pre = $('<pre>').addClass("prettyprint");

            var lines = data.split("\n");
            var diffText = "";

            $.each(lines, function(line, val) {
                if (line > 1) {

                    var span = $('<span>');

                    if (val.indexOf("@@") == 0) {
                        span.addClass("diff_header");
                    }
                    else if (val.indexOf("+") == 0) {
                        span.addClass("diff_added");
                    }
                    else if (val.indexOf("-") == 0) {
                        span.addClass("diff_removed");
                    }

                    val = prettyPrintOne((val + "\n").replace(/[<]/g, "&lt;")
                            .replace(/[>]/g, "&gt;"));
                    span.html(val);
                    pre.append(span);
                }
                else
                    pre.append($('<span>').addClass("diff_header").html(val));
            });

            container.append(pre);

            if (showAsPopup) {

                var pop = new modalPopup("#backgroundPopup");

                $('body').prepend(pop.create(lastURLSeg(url), container));
                pop.show();
            }

        },
        error: function(d, s, x) {
            $('#maincontent').removeClass('spinning');
            reportError(null, d, s, x);

        }
    });

}

function displayFile(hid, path, file) {

    var file = file || lastURLSeg(path);

    var url = findBaseURL("changesets") + "/blobs/" + hid + "/" + file;

    if (isMergeable(file)) {
        $('#maincontent').addClass('spinning');
        var pre = $('<pre>').addClass("prettyprint");

        $.ajax({
            url: url,
            cache: false,
            success: function(data, status, xhr) {

                $('#maincontent').removeClass('spinning');

                var content = prettyPrintOne(data.replace(/[<]/g, "&lt;")
                                         .replace(/[>]/g, "&gt;"));

                try {
                    pre.html(content);
                }
                catch (e) {
                    pre.text(data);
                }
                var div = $('<div>');
                var span = $('<span>').attr({ id: "newwindow" });
                var rv = $('<a>').text("raw view").attr({ href: url, target: "new" });
                span.append(rv);
                //div.append(span);
                div.append(pre);

                var pop = new modalPopup("#backgroundPopup");

                $('body').prepend(pop.create((path ? path : file), div, span));
                pop.show();

            },
            error: function(d, s, x) {
                $('#maincontent').removeClass('spinning');

            }
        });


    }
    else {
        window.open(url);
    }

}

function getRepos(afterSuccessFunction) {

    $('#maincontent').addClass('spinning');

    var url = "/repos/";
    $.ajax(
    {
        url: url,
        dataType: 'json',
        cache: false,
        error: function(data, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {
            $('#maincontent').removeClass('spinning');

            if ((status != "success") || (!data)) {
                return;
            }
            afterSuccessFunction(data);
        }

    });

}
