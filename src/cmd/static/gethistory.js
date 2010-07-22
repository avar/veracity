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

var currentNumber = 0;
var dataChunk = 50;
var allLoaded = false;
var historyItems = [];
var scrollPosition = 0;
var origURL = null;
var curURL = null;
var stampsLoaded = false;
var usersLoaded = false;



function resetHistory() {

    $('#tablehistory tr').remove();

    allLoaded = false;
    historyItems = [];
    currentNumber = 0;
    curURL = origURL;

}

function getQueryStringPair(qs, key, value) {

    var str = "";
    str = (qs.length == 0 ? "?" : "&") + key + "=" + value;
    return str;    

}

function filterHistory() {

    var qs = "";
    var from = $('#datefrom').val();
    if (from)
        qs += getQueryStringPair(qs, "from", encodeURI(from));
    var to = $('#dateto').val();
    if (to)
        qs += getQueryStringPair(qs, "to", encodeURI(to));
    var user = $('#user').val();
    if (user)
        qs += getQueryStringPair(qs, "user", encodeURI(user));
    var stamp = $('#stamp').val();
    if (stamp)
        qs += getQueryStringPair(qs, "stamp", encodeURI(stamp));

    resetHistory();

    var url = window.location.pathname + qs;

    curURL = url;
    getHistory(50);

}

function clearFilter() {
    resetHistory();
    $('#datefrom').val("");
    $('#dateTo').val("");
    $('#user').val("");
    $('#stamp').val("");
    getHistory(50);
}

function getUsers() {

    var url = findBaseURL("history") + "/users/records";

    var stamps = [];
    $.ajax({
        url: url,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {
            if ((status != "success") || (!data)) {
                return;
            }

            var select = $('#user');
            select.append($('<option>').text(""));
            $.each(data, function(i, v) {
                var option = $('<option>').text(v.email).val(v.recid).attr({id: v.recid});
                
                select.append(option);

            });
    
            select.val("");
        }

    });

}

function getAllStamps() {

    $('#maincontent').css("height", "300px");
    $('#maincontent').addClass('spinning');

    var str = "";

    var url = findBaseURL("history") + "/stamps/";

    var stamps = [];
    $.ajax({
        url: url,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {

            $('#maincontent').removeClass('spinning');
            $('#maincontent').css("height", "");
            if ((status != "success") || (!data)) {
                return;
            }
            $.each(data, function(i, v) {

                stamps.push(v.stamp);
            });
            $("#stamp").autocomplete({
                source: stamps,
                minLength: 3
            });
        }

    });
  
}


function loadHistory() {

    if (historyItems.length > currentNumber) {
        var showItems = historyItems.slice(currentNumber, currentNumber += dataChunk);

        $.each(showItems,
			     function(index, value) {
			         var tr = $('<tr>');
			         var tdt = $('<td>');
			         var tds = $('<td>');
			         var td1 = $('<td>');
			         var td2 = $('<td>');
			         var td3 = $('<td>');
			         var td4 = $('<td>');
			         var when_array = [];
			         var who_array = [];

			         $.each(value.audits, function(i, ts) {

			             var d = new Date(parseInt(ts.when));
			             when_array.push(shortDateTime(d));
			             var who = ts.email || ts.who;
			             who_array.push(who);
			         }
			         );

			         td1.text(when_array.join(" "));
			         td2.text(who_array.join(" "));

			         if (value.comments && value.comments.length > 0) {
			             //history is turned off right now which messes this up
			             //so check to see if we have history
			             if (value.comments[0].history) {
			                 value.comments.sort(
		                    function(a, b) {
		                        return (a.history[0].audits[0].when - b.history[0].audits[0].when);
		                    });
			             }
			             var spancomment = $('<div>').text(value.comments[0].text);

			             td3.prepend(spancomment);
			         }

			         if (value.tags != null) {

			             $.each(value.tags, function(i, v) {

			                 var span = $('<span>').append(v).addClass("tag");

			                 tdt.addClass("tdcenter").append(span);

			                 if (i > 1 && (i % 5 == 0)) {
			                     tdt.append($('<br/>'));
			                 }

			             });
			         }
			         if (value.stamps != null) {

			             $.each(value.stamps, function(i, v) {
			                 var a = $('<a>').text(v).attr({ "title": "view items with this stamp", href: findBaseURL("history") + "/history?stamp=" + v });

			                 var span = $('<span>').append(a).addClass("stamp");

			                 tds.addClass("tdcenter").append(span);
			                 if (i > 1 && (i % 5 == 0)) {
			                     tds.append($('<br/>'));
			                 }
			             });
			         }

			         var urlBase = window.location.pathname.rtrim("/").rtrim("history").rtrim("/");

			         td4.append(makePrettyRevIdLink(value.changeset_id, true, null, null, urlBase));
			         tr.append(tdt);
			         tr.append(td4);
			         tr.append(td3);
			         tr.append(tds);
			         tr.append(td1);
			         tr.append(td2);

			         $('#tablehistory').append(tr);

			     });

        if (historyItems.length != dataChunk && currentNumber >= historyItems.length)
            $('#more').hide();

        $('#tbl_footer').removeClass('spinning');

        $("table.datatable tr").hover(
			            function() {
			                $(this).css("background-color", "#E4F0CE");
			            },
                        function() {
                            $(this).css("background-color", "white");
                        });


    }
}

function getHistory(maxRows) {
    $(window).scrollTop(scrollPosition);

    $('#maincontent').css("height", "300px");
    $('#maincontent').addClass('spinning');

    var str = "";

    var qs = window.location.href.slice(window.location.href.indexOf('?'));
    str = getQueryStringPair((qs[1] ? qs[1] : ""), "max", maxRows);

    var url = curURL || window.location.href + str;
   
    $.ajax({
        url: url,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {

            $('#maincontent').removeClass('spinning');
            $('#maincontent').css("height", "");
            if ((status != "success") || (!data)) {
                return;
            }

            historyItems = data;
            loadHistory();
            $(window).scroll(scrollPosition);

        }
    });

}


function sgKickoff() {

    $('#more').click(function(e) {
        $('#tbl_footer').addClass('spinning');

        e.stopPropagation();
        e.preventDefault();

        if (!allLoaded)
            getHistory(0);
        else
            loadHistory();

        allLoaded = true;

        $(window).scroll(scrollPosition);

    });

    $(window).scroll(function() {

        scrollPosition = $(window).scrollTop();
        if (isScrollBottom()) {

            if (allLoaded) {
                loadHistory();
            }
        }
    });


    
    $('#datefrom').datepicker({ dateFormat: 'yy-mm-dd', changeYear: true, changeMonth: true });
    $('#dateto').datepicker({ dateFormat: 'yy-mm-dd', changeYear: true, changeMonth: true });

    var a = $('#toggle_filter');
    a.toggle(function() {

        $('#imgtoggle').attr("src", "/static/css/images/closegreen.png");
        $('#filter').show();
        if (!stampsLoaded) {
            getAllStamps();
        }
        if (!usersLoaded) {
            getUsers();
        }
    },
    function() {

        $('#imgtoggle').attr("src", "/static/css/images/opengreen.png");
        $('#filter').hide();
    });
    $('#filter').hide();
  
    origURL = window.location.href;
    getHistory(50);

    $('#historyfilter').submit(filterHistory);
}



