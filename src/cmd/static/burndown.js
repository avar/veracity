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

var usersLoaded = false;
var _sprints = {};


function _chart(data, user, startDate, endDate)
{
    var curDate = startDate;

    var applied = 0;
    var total = 0;
    var seen = {};
    var outstanding = 0;
    var dayMs = 1000 * 60 * 60 * 24;

    var guideline = [];
    var workdone = [];
    var workleft= [];
    var workper = [];
    var guessworkper = [];
    var burndown = [];
    var guessworkdone = [];
    var guessworkleft = [];
    var lastDate = startDate;
    var lastN = 0;
    var highWater = 0;

    var periodDays = Math.floor((endDate.getTime() - curDate.getTime()) / dayMs);

    var n = 0;

    while (curDate.getTime() <= endDate.getTime())
    {
        var dateStr = curDate.getFullYear() + "/" + stretch(curDate.getMonth() + 1) +
            "/" + stretch(curDate.getDate());

        if (!! data[dateStr])
        {
            var items = data[dateStr];

            for ( var recid in items )
            {
                var rec = items[recid];

                if ((!! user) && ((! rec.assignee) || (rec.assignee != user)))
                    continue;

                var prevWork = 0;
                var prevAllotted = 0;

                if (!! seen[recid])
                    {
                        prevWork = seen[recid].original_estimate || 0;
                        prevAllotted = seen[recid].logged_time || 0;
                    }

                applied += (rec.logged_time || 0) - prevAllotted;
                total += (rec.original_estimate || 0) - prevWork;
                highWater = Math.max(highWater, total);

                seen[recid] = rec;
            }

            outstanding = (total - applied);

            workdone.push([curDate.getTime(), applied]);
            workleft.push([curDate.getTime(), outstanding]);

            if (n < periodDays)
            {
                var hoursPer = Math.ceil((outstanding / (periodDays - n)) * 10) / 10.0;
                workper.push([curDate.getTime(), hoursPer]);
            }

            lastDate = curDate;
            lastN = n;
        }

        curDate = new Date(curDate.getTime() + (dayMs));
        ++n;
    }

    curDate = new Date(lastDate.getTime() + dayMs);
    n = lastN + 1;

    if ((curDate.getTime() <= endDate.getTime()) && (workper.length > 0))
    {
        guessworkper.push(workper[workper.length - 1]);
        guessworkdone.push(workdone[workdone.length - 1]);
        guessworkleft.push(workleft[workleft.length - 1]);
    }

    while (curDate.getTime() <= endDate.getTime())
    {
        var hoursPer = 0;

        if (n < periodDays)
        {
            hoursPer = Math.round((outstanding / (periodDays - n)) * 10) / 10.0;
        }
        guessworkper.push([curDate.getTime(), hoursPer]);


        applied += hoursPer;
        if (applied > total)
            applied = total;
        outstanding = (total - applied);

        guessworkdone.push([curDate.getTime(), applied]);
        guessworkleft.push([curDate.getTime(), outstanding]);

        curDate = new Date(curDate.getTime() + (dayMs));
        ++n;
    }

    var lines = [
        {
            data: [[startDate.getTime(),highWater], [endDate.getTime(), 0]], color: "red", lineWidth: 1, shadowSize: 0
        },
        {
            label: "effort", data: workper, color: "darkorange"
        },
        {
             data: guessworkper, color: "yellow"
        },
        {
            label: "done", data: workdone, color: "darkblue"
        },
        {
            label: "left", data: workleft, color: "green"
        },
        {
            data: guessworkleft, color: "lightgreen"
        },
        {
            data: guessworkdone, color: "lightblue"
        }
    ];

    var ctr = $('#burndown')[0];

    var cheight = $(document).height() - 20 - $(ctr).offset().top;
    var cwidth = $(window).width() - 30 - $(ctr).offset().left;

    ctr.style.height = cheight + 'px';
    ctr.style.width = cwidth + 'px';


    var plot = $.plot(ctr, lines,
                      {
                          xaxis: { tickSize : [7, "day"],
                                   mode: "time",
                                   timeformat: "%b %d"
                                 },
                          yaxis: { max: highWater },
                          points: { show: true, fill: false },
                          lines: {
                              show: true
                          }
                      });
}


function _showChart(sprintid)
{
    if ((! sprintid) || (sprintid == ''))
        {
            $('#burndown').empty();
            return;
        }

    var wurl = '/repos/' + startRepo + '/withistory?sprint='
        + sprintid;

    $.ajax(
        {
            url: wurl,
            dataType: 'json',

            success: function(data) {

                var cc = function() {
                    _chart(data, $('#user').val(),
                           new Date(Math.floor(_sprints[sprintid].startdate)),
                           new Date(Math.floor(_sprints[sprintid].enddate)));
                };

                cc();

                $('#user').change(cc);

            },

            error: function(xhr, textStatus, errorThrown) {
//                alert("error: " + textStatus);
            }
        }
    );
}

function getSprints() {

    var uurl = '/repos/' + startRepo + "/wit/records?rectype=sprint";

    $.ajax({
        url: uurl,
        dataType: 'json',
        cache: false,
        error: function(xhr, textStatus, errorThrown) {
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {
            if ((status != "success") || (!data)) {
                return;
            }

            _sprints = [];

            var select = $('#sprint');

            $.each(data, function(i, v) {
                var option = $('<option>').text(v.name + ": " + v.description).val(v.recid).attr({id: v.recid});

                select.append(option);
                _sprints[v.recid] = v;
            });

            select.change(
                function(e) {
                    _setCookie('vvsprint', select.val(), 30);
                    _showChart(select.val());
                }
            );

            var sprt = _findCookie('vvsprint');

            if ((!! sprt) && (sprt != ''))
            {
                select.val(sprt);
                _showChart(sprt);
            }
        }

    });

}

function getUsers() {

    var uurl = '/repos/' + startRepo + "/users/records";

    var stamps = [];
    $.ajax({
        url: uurl,
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

            $.each(data, function(i, v) {
                var option = $('<option>').text(v.email).val(v.recid).attr({id: v.recid});

                select.append(option);

            });

            $("#user").val("");

        }

    });

}

function sgKickoff()
{
    getUsers();
    getSprints();
}

