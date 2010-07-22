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

var _sgTodo = {};


var _theList = false;
var _nextid = 1;
var _workqueue = [];
var _idToGuid = {};
var _workInProgress = false;

function _logKey(e) {
//    _debugOut(e.type + ": " +
//        ", which: " + e.which +
//        ", keyCode: " + e.keyCode +
//        ", charCode: " + e.charCode);
}

function _addBelow(guid, prev, title, status, totalwork, workdone) {
    var n = _nextid++;
    var ourid = "vvtodo" + n;

    var li = $(document.createElement('li'));
    li.addClass(status);
    li.attr('id', ourid);
    _idToGuid[ourid] = guid;

    var sp = $(document.createElement('span'));
    sp.text(' ');
    sp.addClass('handle');
    li.append(sp);

    li.draggable( { containment: 'parent' } );

    var cb = $(document.createElement('input'));
    cb.attr('type', 'checkbox');
    if ((!! status) && (status != 'open'))
        cb.attr('checked', 'checked');
    cb.click(
       function (e) {
	   var newStatus = '';
	   if (cb.is(':checked'))
    	   {
	       li.removeClass('verified');
	       li.removeClass('open');
	       li.addClass('fixed');
	       newStatus = 'fixed';

	       li.fadeOut('slow');
	   }
	   else
	   {
	       li.removeClass('verified');
	       li.removeClass('fixed');
	       li.addClass('open');
	       newStatus = 'open';
	   }

	   _updateItemField(guid, 'status', newStatus);
       }
    );

    li.append(cb);

    var inp = $(document.createElement('textarea'));
    inp.val(title);
    inp.attr('rows', 1);

    inp.dblclick(
        function(e) {
            e.preventDefault();

            var g = _idToGuid[ourid];
            if (!!g) {
                var u = '/repos/' + startRepo + '/wit/records/' + g;
                window.document.location.href = u;
            }
        }
    );

    var ignoreKeys = function(e) {
        _logKey(e);

        switch (e.which || e.keyCode) {
        case 13:
        case 38:
        case 40:
            e.preventDefault();
            e.stopPropagation();
            break;
        }
    };

    inp.keyup( ignoreKeys );
    inp.keypress( ignoreKeys );

    inp.keydown(
	function(e) {
	    _logKey(e);

	    switch (e.which || e.keyCode) {
	    case 13:
	        e.preventDefault();
	        e.stopPropagation();

	        if (jQuery.trim(inp.val()) != "") {
	            _updateWorkItem(li);

	            _addAtEnd();
	        }
	        break;

	    case 38:
	        e.preventDefault();
	        e.stopPropagation();
	        // move up
	        if (li.prev()) {
	            li.prev().before(li);
	            _juggleOrder(_orderedGuidList(), _sgTodo);
	            inp.focus();
	        }
	        break;

	    case 40:
	        // move down
	        e.preventDefault();
	        e.stopPropagation();

	        if (li.next()) {
	            li.next().after(li);
	            _juggleOrder(_orderedGuidList(), _sgTodo);
	            inp.focus();
	        }
	        break;
	    }
	}
    );

    inp.blur(
	function(e) {
	    _updateItemField(guid, 'title', inp.val());
	}
    );

    li.append(inp);

    if ((!! totalwork) && (totalwork > 0))
    {
        var partial = workdone || 0;
        li.append(" <span class='todowork'>" + partial + " / " + totalwork + "</span>");
    }

    if (!! prev)
	prev.after(li);
    else
	_theList.append(li);

    // do this here so we get resized right away
    autoResizeText(inp, 2000);

    return(li);
}

function _updateItemField(guid, vari, value)
{
    if ((!! _sgTodo[guid]) &&
	((! _sgTodo[guid][vari]) || (_sgTodo[guid][vari] != value))
       )
    {
	_sgTodo[guid][vari] = value;

	_debugOut("added " + guid + " to work queue");
	_pendUpdate(guid);
    }
}


function _pendUpdate(guid) {
    var item = _sgTodo[guid];
    delete item.recid;
    delete item.rectype;

    var uurl = '/repos/' + startRepo + '/wit/records/' + guid;

    var j = JSON.stringify(item);

    _debugOut("pend " + j);

    var ajaxAction =
	{
	    url: uurl,
	    dataType: 'text',
	    contentType: 'application/json',
	    processData: false,
	    type: 'PUT',
	    data: j,

	    beforeSend: function(xhr) {
	        _workInProgress = true;
	        _debugOut("set work-in-progress true");
	        xhr.setRequestHeader('From', sgUserName);
	    },

	    error: function(xhr, textStatus, errorThrown) {
	        _workInProgress = false;
	        _debugOut("set work-in-progress false on error");
	        _getToWork();
	    },

	    success: function(data) {
	        _workInProgress = false;
	        _debugOut("set work-in-progress false on success");
	        _getToWork();
	    }
	};

	_workqueue.push(ajaxAction);
	_getToWork();
}

function _getToWork()
{
    if (_workqueue.length == 0) {
        _debugOut("empty work queue, exiting");
        return;
    }

    if (_workInProgress) {
        _debugOut("work in progress, waiting");
        return;
    }

    var ajaxAction = _workqueue.shift();

    _debugOut("starting action " + ajaxAction.url);
    $.ajax(ajaxAction);
}

function _addAtEnd()
{
    var lastOne = false;
    var lastSeen = 0;
    var lastInList = false;

    $('#todolist li').each(
	function() {
	    var id = $(this).attr('id');
	    var g = _idToGuid[id];
	    lastInList = $(this);

	    if ((!! g) && (!! _sgTodo[g]))
	    {
		var o = _num(_sgTodo[g].listorder) + 0;
		if (o > lastSeen)
		    lastSeen = o;
	    }

	    var tv = $(this).children('textarea').val();

	    if ($.trim(tv) == "")
	    {
		lastOne = this;
	    }
	}
    );

    if (! lastOne)
    {
	_addBelow('', lastInList, '', 'open').children('textarea').focus();
    }
}


function _initList(container) {
    _theList = $(document.createElement('ul'));
    _theList.addClass('todolist');

    _theList.empty();
    $(container).append(_theList);

    var purl = '/repos/' + startRepo + '/wit/records/?assignee=' + sgUserName;

    var prev = false;
    var guidsInOrder = [];

    $.ajax(
    {
        url: purl,
        dataType: 'json',

        error: function(xhr, textStatus, errorThrown) {
        },

        success: function(wits) {

	    for ( var j = wits.length - 1; j > 0; --j )
	    {
		if ((wits[j].rectype != 'item') ||
		    (wits[j].status == 'verified') ||
		    (wits[j].status == 'fixed')
		   )
		    wits.splice(j, 1);
	    }

            for (var i in wits) {
                var wit = wits[i];

		guidsInOrder.push(wit.recid);
		_sgTodo[wit.recid] = wit;
            }

	    _sortByOrder(guidsInOrder);
	    _juggleOrder(guidsInOrder, _sgTodo);

	    for ( var i in guidsInOrder )
	    {
		var g = guidsInOrder[i];
		var item = _sgTodo[g];

		prev = _addBelow(g, prev, item.title, item.status, item.original_estimate, item.logged_time);
	    }

	    prev = _addBelow('', prev, '', 'open');
	    $(prev).children('textarea').focus();
        }
    });


    _theList.droppable(
     { drop:
	function(e, ui) {
	    var dragged = ui.draggable;

	    var lastBefore = false;
	    var firstAfter = false;

	    var dtop = $(dragged).position().top;

	    _theList.children().each(
		function () {
		    var ctop = $(this).position().top;

		    if (ctop < dtop)
		    {
    			lastBefore = $(this);
		    }
		    else if ((ctop > dtop) && ! firstAfter)
		    {
		        firstAfter = $(this);
		    }
		}
	    );

	    if (lastBefore)
		$(lastBefore).after(dragged);
	    else if (firstAfter)
	        $(firstAfter).before(dragged);

	    dragged[0].style.removeProperty('top');
	    dragged[0].style.removeProperty('left');

	    _juggleOrder(_orderedGuidList(), _sgTodo);
	}
}
    );

}


function _sortByOrder(items)
{
    items.sort(
	function(ga, gb) {
	    var a = _sgTodo[ga];
	    var b = _sgTodo[gb];

	    if ((!! a.listorder) && (!! b.listorder))
	    {
		a.listorder = _num(a.listorder);
		b.listorder = _num(b.listorder);
		return(a.listorder - b.listorder);
	    }
	    else if (!! a.listorder)
	    {
		return(-1);
	    }
	    else if (!! b.listorder)
	    {
		return(1);
	    }
	    else
	    {
		return(0);
	    }
	}
    );
}

function _orderedGuidList()
{
    var list = [];

    $('#todolist li').each(
	function() {


	var id = $(this).attr('id');
	var g = _idToGuid[id];
	list.push(g);
	}
    );

    return(list);
}

function _createWorkItem(listid, textval)
{
    var purl = '/repos/' + startRepo + '/wit/records/';

    var wi = {
	rectype: 'item',
	title: textval,
	status: "open",
        assignee: sgUserName
    };

    var nextli = _juggleOrder(_orderedGuidList(), _sgTodo);

    wi.listorder = nextli + 1;

    var j = JSON.stringify(wi);

    var ajaxAction =
	{
	    url: purl,
	    dataType: 'text',
	    contentType: 'application/json',
	    processData: false,
	    type: 'POST',
	    data: j,

	    beforeSend: function(xhr) {
	        xhr.setRequestHeader('From', sgUserName);
	        _debugOut("_createWorkItem " + j);
	    },

	    error: function(xhr, textStatus, errorThrown) {
	        _debugOut("_createWorkItem error " + j);
	    },

	    success: function(data) {
	        _debugOut("_createWorkItem success " + j);
	        _idToGuid[listid] = data;

	        _sgTodo[data] = wi;

	        _juggleOrder(_orderedGuidList(), _sgTodo);
	    }
	};

	_workqueue.push(ajaxAction);
	_getToWork();
}

function _updateWorkItem(li)
{
    var oldId = $(li).attr('id');
    var val = $(li).children('textarea').val();
    val = jQuery.trim(val);

    var g = _idToGuid[oldId];

    if (! g)
    {
	_createWorkItem(oldId, val);
    }
    else
    {
	// later
    }
}

function _juggleOrder(orderedGuids, items)
{
    var lastSeen = 0;

    for (i in orderedGuids)
    {
	var g = orderedGuids[i];

	if ((!! g) && (!! items[g]))
	{
	    if (_num(items[g].listorder) <= lastSeen)
	    {
		_updateItemField(g, 'listorder', lastSeen + 1);
	    }

	    lastSeen = _num(items[g].listorder) + 0;
	}
    }

    return(lastSeen + 1);
}


function _num(val)
{
    if (! val)
	return(0);

    if (typeof val == "string")
	return(parseInt(val));

    return(val);
}

function sgKickoff() {
    _initList(document.getElementById('todolist'));
}
