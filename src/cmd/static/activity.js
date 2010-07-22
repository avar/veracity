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

var sgLastActivityMod = false;

//  build an activity <li /> out of a who/what/when hash
//
function activityItem(activity) {
    var li = $(document.createElement("li"));

    var p = $(document.createElement("p"));
    p.addClass('act');

    p.text(activity.what);

    li.append(p);

    p = $(document.createElement("p"));
    p.addClass('actsig');

    var d = new Date(parseInt(activity.when));
    var who = activity.email || activity.who;
    p.text(_shortWho(who));
    p.append($('<br/>'));

    if ((!! activity.link) && ($.trim(activity.link) != ""))
    {
        var ln = '/repos/' + startRepo + activity.link;
	var a = $(document.createElement('a'));
	a.attr('href', ln);
	a.text(shortDateTime(d));
	p.append(a);
    }
    else
    {
        p.append(shortDateTime(d));
    }

    li.append(p);

    return (li);
}


function listActivity() {

    var listUrl = "/repos/" + encodeURI(startRepo) + "/activity/";

    $('#activitylist').addClass('spinning');

    $.ajax(
    {
        url: listUrl,
        dataType: 'json',
        cache: false,
        beforeSend: function(xhr) {
            if (sgLastActivityMod) {
                xhr.setRequestHeader('If-Modified-Since', sgLastActivityMod);
            }
        },

        error: function(xhr, textStatus, errorThrown) {
            $('#activitylist').removeClass('spinning');
	    window.setTimeout(listActivity, 30000);
        },

        success: function(data, status, xhr) {
            $('#activitylist').removeClass('spinning');
	    window.setTimeout(listActivity, 30000);

            if ((status != "success") || (!data)) {
                return;
            }

            var lm = xhr.getResponseHeader("Last-Modified");

            if (lm) {
                sgLastActivityMod = lm;
            }
            else {
                sgLastActivityMod = false;
            }

            var uls = $('#activitylist ul');
            var ul = false;

            if (uls.length > 0) {
                ul = uls[0];
            }
            else {
                ul = $(document.createElement("ul"));
                $('#activitylist').append(ul);
            }

            data.sort(
		       function(a, b) {
		           return (a.when - b.when);
		       }
		    );

            var li = false;

            $.each(data,
			  function(index, activity) {
			      li = activityItem(activity);

			      $(ul).prepend(li);
			  }
            );

			$('li').remove('.sentact');

            var lis = $(ul).children('li');

            if (lis.length > 0) {
                $(lis[lis.length - 1]).addClass('lastact');
            }

            _actPager.init('#activitylist', '#activitynav');
        }

    });


}

function sendNewAct(e)
{
    e.preventDefault();

    var newText = $('#whatidid').val();

    if (!newText)
    {
        return;
    }

    var submission = {
        "rectype" : "item",
        "what": newText,
        "who": sgUserName,
        "when": (new Date()).getTime()
    };

	var stext = JSON.stringify(submission);

	$.ajax(
	{
	    "type": "POST",
	    "url": "/repos/" + startRepo + "/activity/records",
	    "data": stext,
	    "dataType": "json",
	    "processData": false,
	    "beforeSend": function(xhr)
	    {
	        xhr.setRequestHeader('From', sgUserName);
	    }
	});

    var li = activityItem(submission);
    $(li).addClass('sentact');

    $('#activitylist ul').prepend(li);
    _actPager.redisplay();
}

var _actPager =
{
    box: false,
    curTop: 0,

    init: function(ctr, nav) {
        this.box = $(ctr);
        this.nav = $(nav);
        this.box.attr('overflow', 'hidden');
        this.alist = $(ctr + ' ul')[0];
        this.redisplay();
    },

    redisplay: function() {
        var top = this.box.offset().top;
        var wheight = $(window).height() || window.innerHeight;
        var bheight = wheight - top - 30;
        var hidingAfter = -1;
        var pager = this;

        this.box.height(bheight);

        $(this.alist).children('li').each(
            function (idx, li) {
                if ((idx < pager.curTop) || (hidingAfter >= 0))
                {
                    $(li).hide();
                }
                else
                {
                    $(li).show();

                    var bottom = $(li).offset().top + $(li).height() - top;

                    if (bottom >= bheight)
                    {
                        $(li).hide();
                        hidingAfter = idx;
                    }
                }
            }
        );

        pager.nav.empty();

        if (pager.curTop > 0)
        {
            var ln = document.createElement('a');
            ln.href = '#';
            $(ln).text('prev');
            $(ln).addClass('prevactivity');

            $(ln).click(
                function (e) {

                    e.stopPropagation();
                    e.preventDefault();

                    pager.backUp();
                }
            );

            pager.nav.append(ln);
        }

        if (hidingAfter >= 0)
        {
            var ln = document.createElement('a');
            ln.href = '#';
            $(ln).text('next');
            $(ln).addClass('nextactivity');

            $(ln).click(
                function (e) {

                    e.stopPropagation();
                    e.preventDefault();

                    pager.curTop = hidingAfter;
                    pager.redisplay();
                }
            );


            pager.nav.append(ln);
        }


    },

    backUp: function() {
        var top = this.box.offset().top;
        var wheight = $(window).height() || window.innerHeight;
        var bheight = wheight - top - 30;
        var hidingAfter = -1;
        var pager = this;
        var tooFar = false;
        var i;

        this.box.height(bheight);

        var els = $(this.alist).children('li');

        i = this.curTop - 1;
        var canary = $(els[i]);

        this.curTop = 0;

        while ((i > 0) && (! tooFar))
        {
            var testee = els[i];
            $(testee).show();

            var bottom = canary.offset().top + canary.height() - top;

            if (bottom >= bheight)
            {
                this.curTop = i + 1;
                break;
            }

            --i;
        }

        this.redisplay();
    }
};
function _setShowActivity(show) {
    var act = "sgshowactivity";  

    sgShowActivity = show;    

    _setCookie(act, '', -1);
    _setCookie(act, show, 30);

}

function _showActivity() {
    return (_findCookie("sgshowactivity"));
}

function hideActivity() {

    $('#imghideact').attr("src", "/static/css/images/doubleopenblue.png");
    $('#activity p').hide();  
    $('#activity').width(20);
    $('#maincontent').css("left", "36px");
    $('#newactivity').hide();

}

function displayActivity() {
    $('#activity p').show();
    $('#imghideact').attr("src", "/static/css/images/doublecloseblue.png");
    $('#activity').width(214);
    $('#maincontent').css("left", "230px");
    $('#newactivity').show();

}
$(document).ready(function() {

    $('#linkhideact').toggle(
    function() {
        _setShowActivity(false);
        hideActivity();

    },
    function() {
        _setShowActivity(true);
        displayActivity();
        listActivity();

    });

    if (sgShowActivity == "true") {
        listActivity();
    }
    else {
        hideActivity();
    }
});                         // at startup

$('#newactivity').submit(sendNewAct);   // every time the submit button is clicked
