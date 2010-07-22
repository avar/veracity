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

var _zingPieces = {};
var _wits = {};
var _sets = {};
var _template = {};
var _links = [];
var _wiListDirty = false;
var _wiusers = {};
var _scaling = {};
var _sprints = {};
var _filters = {};
var _comments = {};



/**
 * create a form to edit a Zing template
 *
 * @param template Zing template structure, already eval'ed (*not* a JSON string)
 * @param container block-level DOM object (div, etc.) to contain the form. will be emptied and filled with the form
 */
 function zingForm(template, container, disable) {


    if (! template.rectypes)
    {
	throw("no rectype found in template");
    }

     _template = template;

    var fieldsets = [];
    _zingPieces = {};

    for ( var rt in template.rectypes ) {
        _zingPieces[rt] = {};

	var fs = _zingFieldset(rt, template.rectypes[rt], disable);

	if (! fs.is(':empty'))
	    fieldsets.push(fs);
    }

    var f = $(document.createElement('form'));

    for ( var i = 0; i < fieldsets.length; ++i )
    {
	f.append(fieldsets[i]);

	if ((i % 2) == 0)
	    fieldsets[i].addClass('even');
    }

     var sf = $(document.createElement('div'));
     var addsb = $(document.createElement('input'));
     addsb.attr('type', 'submit');

     addsb.attr('value', 'add');
     addsb.attr('id', 'zfadditem');
     sf.append(addsb);

     var updatesb = $(document.createElement('input'));
     updatesb.attr('type', 'submit');
     updatesb.hide();
     updatesb.attr('value', 'update');
     updatesb.attr('id', 'zfupdateitem');
     sf.append(updatesb);

     var recidfield = $(document.createElement('input'));
     recidfield.attr('type', 'hidden');
     recidfield.attr('name', 'recid');
     recidfield.attr('id', 'zfrecid');
     sf.append(recidfield);

     f.append(sf);

     addsb.click(
	 function(e) {
	     e.stopPropagation();
	     e.preventDefault();
             _wiListDirty = true;
	     _addWorkItem();
	 }
     );

     updatesb.click(
	 function(e) {
	     e.stopPropagation();
	     e.preventDefault();
             _wiListDirty = true;
	     _updateWorkItem();
	 }
     );

    $(container).empty();
    $(container).append(f);

    if (sgZfAction == "list")
        _zingListWorkItems();
}

function _scaleOut(fn, v)
{
    if (!! _scaling[fn])
    {
        return( Math.floor(v * 1.0 * _scaling[fn]) );
    }

    return(v);
}

function _scaleIn(fn, v)
{
    if (!! _scaling[fn])
    {
        return( Math.floor((v * 100.0) / _scaling[fn]) / 100.0 );
    }

    return(v);
}

function _getWitFields(rt)
{
    var fields = _zingPieces[rt];
    var newRec = {};
    newRec.rectype = rt;

    for ( var fn in fields )
    {
	var fld = fields[fn];

	var val = $(fld).val();

	if (val) {
        val = _scaleOut(fn, val);

	    newRec[fn] = val;
	}
    }

    return(newRec);
}

function _addWorkItem()
{
    var recs = [];

    var rt = 'item';

    var csfld = $('#zing_vc_changeset_commit');
    var csval = false;

    if (!!csfld)
        csval = csfld.val();

    var newRec = _getWitFields(rt);

    for ( var ii = 0; ii < _links.length; ++ii )
    {
        var l = _links[ii];

        var v = $('#' + l.id).val();

	if (v)
	    newRec[l.to] = v;
    }

    var j = JSON.stringify(newRec);
    $('#zingform').addClass('spinning');

    $.ajax(
    {
        url: sgZfBaseUrl + "records",
        dataType: 'text',
        contentType: 'application/json',
        processData: false,
        type: 'POST',
        data: j,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },

        error: function(xhr, textStatus, errorThrown) {
	    $('#zingform').removeClass('spinning');
            _reportError(sgZfBaseUrl + "records", xhr, textStatus, errorThrown);
        },

        success: function(data) {
	    $('#zingform').removeClass('spinning');

	    newRec.recid = data;
	    _wits.push(newRec);

	    if (csval) {
		_associate(data, csval, true);
	    }
	    else
	    {
		var bugurl = sgZfBaseUrl + "/records/" + data;
		window.document.location.href = bugurl;
	    }
        }
    });

}


function _updateWorkItem()
{
    var recs = [];

    var rt = 'item';

    var newRec = _getWitFields(rt);
    var witId = $('#zfrecid').val();
    delete newRec.rectype;

    var j = JSON.stringify(newRec);

    $('#zingform').addClass('spinning');

    $.ajax(
    {
        url: sgZfBaseUrl+"records/" + witId,
        dataType: 'text',
        contentType: 'application/json',
        processData: false,
        type: 'PUT',
        data: j,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },

        error: function(xhr, textStatus, errorThrown) {
	    $('#zingform').removeClass('spinning');

        _reportError(sgZfBaseUrl + "records/" + witId, xhr, textStatus, errorThrown);
    },

        success: function(data) {
	    $('#zingform').removeClass('spinning');

            var i;
            for ( i = 0; i < _wits.length; ++i )
                if (_wits[i].recid == witId)
                    break;

            if (i >= _wits.length)
                return;

    	    for ( var f in newRec )
	        {
		        _wits[i][f] = newRec[f];
    	    }

	    _setLinks(witId);

        }
    });

}

function _setLinks(witId)
{
    var i;
    for ( i = 0; i < _wits.length; ++i )
        if (_wits[i].recid == witId)
            break;

    if (i >= _wits.length)
        return;

    for ( var ii = 0; ii < _links.length; ++ii )
    {
        var l = _links[ii];

        var v = $('#' + l.id).val();
        _wits[i][l.to] = v;

        if ((!! v) && (v != ''))
        {
            var lurl = '/repos/' + startRepo + '/wit/records/' +
                v + "/items";
            var linkdata = { "recid" : witId };

            var j = JSON.stringify(linkdata);

            $.ajax(
                {
                    url: lurl,
                    dataType: 'text',
                    contentType: 'application/json',
                    processData: false,
                    type: 'PUT',
                    data: j,
                    beforeSend: function(xhr) {
                        xhr.setRequestHeader('From', sgUserName);
                    },

                    error: function(xhr, textStatus, errorThrown) {
                        _reportError(lurl, xhr, textStatus, errorThrown);
                    },

                    success: function(data) {
                    }
                });
        }
    }
    
}


    function _associate(bugrecid, csrecid, reload) {
    var linkdata = {
	commit : csrecid
    };

    var j = JSON.stringify(linkdata);

    $.ajax(
    {
        url: sgZfBaseUrl + "records/" + bugrecid + "/changesets",
        dataType: 'text',
        contentType: 'application/json',
        processData: false,
        type: 'PUT',
        data: j,
        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },

        error: function(xhr, textStatus, errorThrown) {
            alert("unable to set work item / changeset association: " + textStatus + "\r\n" + errorThrown);
        },

        success: function(data) {
	    if (reload)
	    {
		var bugurl = sgZfBaseUrl + "/records/" + bugrecid;
		window.document.location.href = bugurl;
	    }
	    else
	    {
		_showChangesets(bugrecid);
	    }
        }
    });
}

function _removeAssociation(csrecid, bugrecid) {
    $.ajax(
    {
        url: sgZfBaseUrl + "records/" + csrecid,
        dataType: 'text',
        type: 'DELETE',

        beforeSend: function(xhr) {
            xhr.setRequestHeader('From', sgUserName);
        },

        error: function(xhr, textStatus, errorThrown) {
            alert("unable to remove work item / changeset association: " + textStatus + "\r\n" + errorThrown);
        },

        success: function(data) {
	    _showChangesets(bugrecid);
        }
    });
}


function _multiLinksTo(fromrectype, torectype)
{
    var details = _findLinkDetails(fromrectype);

    if ((!! details) && (details.to.link_rectypes[0] == torectype))
    {
        return(! details.to.singular);
    }

    return(false);
}


function _singleLinkFrom(fromrectype, torectype)
{
    var details = _findLinkDetails(fromrectype);

    if ((!! details) && (details.to.link_rectypes[0] == torectype))
    {
        return(details.from.singular);
    }

    return(false);
}

function _formIs(fielddef, formvar, value)
{
    return( (!! fielddef.form) &&
	    (typeof fielddef.form[formvar] != "undefined") &&
	    (fielddef.form[formvar] == value));
}

function _zingFieldset(rectype, details, disable)
{
    if (! details.fields)
    {
	throw("rectype " + rectype + " has no fields");
    }

    if (_singleLinkFrom('item', rectype))
    {
        var zf = _zingLinkListField('zing_item', 'item', rectype);
        var fs = $(document.createElement('fieldset'));
        fs.append(zf);
        return(fs);
    }


    var fs = $(document.createElement('fieldset'));
    fs.attr('id', 'zing_' + rectype);

    var c1 = $(document.createElement('div'));
    var c2 = $(document.createElement('div'));

    for ( var fn in details.fields )
    {
	var fielddef = details.fields[fn];
	var adder = false;

	if ((fn == "who") || (fn == "when"))
	    continue;

	if (_formIs(fielddef, 'visible', false))
	    continue;

        if (_multiLinksTo(rectype, 'item'))
	{
    	    adder = document.createElement('input');
	    $(adder).attr('type', 'button');
	    $(adder).val('Add');
	    $(adder).attr('disabled', 'disabled');
	}

        if ((!! fielddef.form) && (!! fielddef.form.scale))
        {
            _scaling[fn] = 1 * fielddef.form.scale;
        }

	var fld = _zingField('zing_' + rectype, fn, fielddef, adder);

	var l = $(document.createElement('label'));
	l.attr('for', fld.id);

	if ((!!fielddef.form) && (!! fielddef.form.label))
	    l.text(fielddef.form.label + ": ");
	else
	    l.text(fn + ": ");

	if ((!! fielddef.form) && (!! fielddef.form.tooltip))
	    l.attr('title', fielddef.form.tooltip);

	if (disable) {
	    $(fld).attr('disabled', true);
	}

	var c = c1;

	if ((!!fielddef.form) && (!! fielddef.form.column) && (fielddef.form.column == 2))
	    c = c2;

	if (fn == 'id')
	    $(fld).attr('type', 'hidden');
	else
	    c.append(l);

	c.append(fld);

	if (adder)
	    c.append(adder);
	if ($(fld).data('list'))
	    c.append($(fld).data('list'));

	if ((!! fielddef.form) && (!! fielddef.form.help))
	{
		var hb = _zingHelpBox(fld, fielddef.form.help);
		c.append(hb);

//		_zingAddHelpTrigger(fld, hb);

	}

        _zingPieces[rectype][fn] = fld;
    }

    if (c2.is(':empty'))
    {
	if (! c1.is(':empty'))
	{
	    c1.addClass('onlycol');
	    fs.append(c1);
	}
    }
    else
    {
	c1.addClass('leftcol');
	c2.addClass('rightcol');
	fs.append(c1);
	fs.append(c2);

	var breaker = $(document.createElement('div'));
	breaker.addClass('colbreak');
	fs.append(breaker);
    }
    

    return(fs);
}

function _zingListField(prefix, fn, fielddef)
{
    var id = prefix + "_" + fn;

    var f = document.createElement('select');
    f.id = id;

    for ( var i in fielddef.constraints.allowed )
    {
	var ov = fielddef.constraints.allowed[i];

	var opt = document.createElement('option');
	$(opt).value = ov;
	$(opt).text(ov);

	$(f).append(opt);
    }

    return(f);
}


function _zingCsField(prefix, fn, fielddef, adder)
{
    var selector = document.createElement('select');
    selector.id = prefix + "_" + fn;
    var def = document.createElement('option');
    def.value = '';
    def.text = '(changeset)';
    $(selector).append(def);

    var hurl = '/repos/' + startRepo + '/history';
    $.ajax(
    {
        url: hurl,
        dataType: 'json',

        success: function(histlist) {
            for (var i = 0; i < histlist.length; ++i) {
                var cset = histlist[i];
                var csid = cset.changeset_id;

		_sets[csid] = cset;

                var when = shortDateTime(new Date(cset.audits[0].when / 1000));
                var who = cset.audits[0].who;
                var desc = "changeset on " + when;

                if ((!!cset.comments) && (cset.comments.length > 0))
                    desc = cset.comments[0].text;

		_sets[csid].desc = desc;

                var o = document.createElement('option');
                o.value = csid;
                o.text = desc;
                o.title = who + ', ' + when; 

                $(selector).append(o);
            }
        }
    });

    if (adder)
    {
	$(adder).attr('disabled', 'disabled');

	$(selector).change(
	    function(e) {
		var crec = $(selector).val();
		var brec = $('#zfrecid').val();

		if (crec && brec)
		{
		    $(adder).click(
			function(e) {
			    e.stopPropagation();
			    e.preventDefault();

			    _associate(brec, crec);
			}
		    );

		    $(adder).removeAttr('disabled');
		}
		else
		{
		    $(adder).attr('disabled', 'disabled');
		}
	    }
	);
    }

    return(selector);
}


function _zingLinkListField(prefix, fromrectype, rectype, adder)
{
    var selector = document.createElement('select');
    selector.id = prefix + "_" + rectype;
    var def = document.createElement('option');
    def.value = '';
    def.text = '(' + rectype + ')';
    $(selector).append(def);

    _links.push( { id : selector.id, from : fromrectype, to : rectype } );

    var hurl = '/repos/' + startRepo + '/wit/records?rectype=' + rectype;
    $.ajax(
    {
        url: hurl,
        dataType: 'json',

        success: function(histlist) {
            for (var i = 0; i < histlist.length; ++i) {
                var rec = histlist[i];
                var recid = rec.recid;
                var name = rec.name;

                var o = document.createElement('option');
                o.value = recid;
                o.text = name;
                o.title = rec.description;

                $(selector).append(o);
            }
        }
    });

    if (adder)
    {
	$(adder).attr('disabled', 'disabled');

	$(selector).change(
	    function(e) {
		var crec = $(selector).val();
		var brec = $('#zfrecid').val();

		if (crec && brec)
		{
		    $(adder).click(
			function(e) {
			    e.stopPropagation();
			    e.preventDefault();

			    _associate(brec, crec);
			}
		    );

		    $(adder).removeAttr('disabled');
		}
		else
		{
		    $(adder).attr('disabled', 'disabled');
		}
	    }
	);
    }

    return(selector);
}


function _getUserList()
{
    var hurl = '/repos/' + startRepo + '/users/records';
    $.ajax(
    {
        url: hurl,
        dataType: 'json',

        success: function(userlist) {
            for (var i = 0; i < userlist.length; ++i) {
                var user = userlist[i];

                _wiusers[user.recid] = user.email;


            }
            _buildZingForm(false);
        },

        error: function() {
            _buildZingForm(false);

        }
    }
    );
}

function _zingUidListField(prefix, fn, fielddef, adder)
{
    var selector = document.createElement('select');
    selector.id = prefix + "_" + fn;
    var def = document.createElement('option');
    def.value = '';
    def.text = '(select a user)';
    $(selector).append(def);

    for ( var uid in _wiusers )
    {
        var uname = _wiusers[uid];

        var o = document.createElement('option');
        o.value = uid;
        o.text = uname;

        $(selector).append(o);
    }

    if (adder)
    {
	$(adder).attr('disabled', 'disabled');

	$(selector).change(
	    function(e) {
		var crec = $(selector).val();
		var brec = $('#zfrecid').val();

		if (crec && brec)
		{
		    $(adder).click(
			function(e) {
			    e.stopPropagation();
			    e.preventDefault();

			    _associate(brec, crec);
			}
		    );

		    $(adder).removeAttr('disabled');
		}
		else
		{
		    $(adder).attr('disabled', 'disabled');
		}
	    }
	);
    }

    return(selector);
}



function _zingField(prefix, fn, fielddef, adder)
{
    if ((!! fielddef.constraints) && (!! fielddef.constraints.allowed))
    {
    	return(_zingListField(prefix, fn, fielddef));
    }
    else if (prefix == "zing_vc_changeset")
    {
        return(_zingCsField(prefix, fn, fielddef, adder));
    }
    else if (fielddef.datatype == "userid")
    {
        return(_zingUidListField(prefix, fn, fielddef));
    }

    var i = false;

    if (_formIs(fielddef, 'multiline', true))
    {
	i = document.createElement('textarea');
	$(i).attr('rows', 4);
	$(i).css('width', '80%');
    }
    else
	i = document.createElement('input');

    i.name = fn;
    i.id = prefix + "_" + fn;

    if ((!! fielddef.form) && (!! fielddef.form.tooltip))
	i.title = fielddef.form.tooltip;

    if ((!! fielddef.form) && (!! fielddef.form.readonly))
	$(i).attr('readonly', 'readonly');

    if (fielddef.datatype == "int")
    {
	i.setAttribute('type', 'number');

	_zingAddCharFilter(i, /[0-9\.\-\+,]/);
    }
    else if (fielddef.datatype == "userid")
    {
	$(i).attr('spellcheck', 'false');
    }

    _zingFieldConstrain(i, fielddef);

    if (adder)
	{
	    _setupLinkAdd(fn, prefix, i, adder);
	}

    return(i);
}


function _setupLinkAdd(fn, prefix, inp, adder)
{
    var prefre = /^zing_(.+)/;

     var matches = prefix.match(prefre);

    if (! matches)
	return;

    var linkname = _findLink(matches[1]);

    if (linkname)
    {
	var ul = document.createElement('ul');
	ul.id = "zing_item_" + linkname;
	$(inp).data('list', $(ul));

	$(adder).click(
	    function (e) {
		e.preventDefault();

		var data = {};

		data[fn] = $(inp).val();
		var jdata = JSON.stringify(data);

		var id = $('#zfrecid').val();

		if (! id)
		    return;

		var lurl = '/repos/' + startRepo + '/wit/records/' + id + '/' + linkname;

		$.ajax(
		    {
			url: lurl,
			dataType: 'text',
			contentType: 'application/json',
			processData: false,
			type: 'PUT',
			data: jdata,
			beforeSend: function(xhr) {
			    xhr.setRequestHeader('From', sgUserName);
			},

			error: function(xhr, textStatus, errorThrown) {
			_reportError(lurl, xhr, textStatus, errorThrown);

},

			success: function(rdata) {
			    var rif = $('#zfrecid');
			    var witId = rif.val();

			    var i;
			    for ( i = 0; i < _wits.length; ++i )
				if (_wits[i].recid == witId)
				    break;

			    if (i >= _wits.length)
				return;

			    data.who = sgUserName;
			    data.when = (new Date()).getTime();

			    if (! _wits[i].comments)
				_wits[i].comments = [];

			    _wits[i].comments.push( data );
			    
			    _zingPopulate($('#zingform'), _wits[i]);

			}
		    });


	    }
	);
    }
}

function _findLinkDetails(fromrectype)
{
    var res = false;

    var dlts = _template['directed_linktypes'];

    if (dlts)
    {
	for ( var lt in dlts )
	{
	    var details = dlts[lt];

	    if (details.from && details.from.name && details.from.link_rectypes)
	    {
		var found = false;

		for ( var i in details.from.link_rectypes )
		{
		    if (details.from.link_rectypes[i] == fromrectype)
		    {
			return(details);
		    }
		}
	    }
	}
    }

    return(false);
}

function _findLink(linkrec)
{
    var details = _findLinkDetails(linkrec);

    if (!! details)
        return(details.to.name);
    else
        return(false);
}

function _zingFieldConstrain(inp, fielddef)
{
    if (! fielddef.constraints)
    {
	return;
    }

    for ( var ct in fielddef.constraints )
    {
	var cv = fielddef.constraints[ct];

	if (ct == "maxlength")
	{
	    $(inp).attr("maxlength", cv);
	}
	else if (ct == "min")
	{
	    $(inp).attr('min', cv);

	    inp.sgValidator = function()
	    {
		if (($(inp).val() + 0) < cv)
		{
		    return("value must be at least " + cv);
		}

		return("");
	    };

	    $(inp).blur( function(e)
		      {
			  var vm = inp.sgValidator();

			  if (vm != "")
			  {

			      e.stopPropagation();
			      e.preventDefault();
			      $(inp).focus();
			      $(inp).addClass('invalidfield');
			      _zingShowError(inp, vm);
			  }
			  else
			  {
			      $(inp).removeClass('invalidfield');
			      _zingHideError(inp);
			  }
		      }
		       );
	}
    }
}

function _zingFindOrCreateErrContainer(inp)
{
    var eid = inp.id + "_err";

    var err = document.getElementById(eid);

    if (! err)
    {
	err = document.createElement('div');
	err.id = eid;
	$(err).addClass('validationerror');
	err.display = 'none';

	$(err).insertAfter(inp);
    }

    return($(err));
}

function _zingShowError(inp, msg)
{
    var err = _zingFindOrCreateErrContainer(inp);

    err.empty();

    err.text(msg);
    err.fadeIn();
}

function _zingHideError(inp)
{
    var err = _zingFindOrCreateErrContainer(inp);

    err.empty();

    err.fadeOut();
}

function _zingAddCharFilter(f, charRegEx)
{
    $(f).keypress(
	function(evt) {
	    var accept = false;

	    var key = evt.keyCode;

	    if (key < 32) // control char? stay out of it
	    {
		accept = true;
	    }
	    else
	    {
		accept = charRegEx.test(String.fromCharCode(key));
	    }

	    if (! accept)
		evt.preventDefault();

	    return(accept);
	}
    );
}

function _zingHelpBox(f, helptext)
{
    var helpBox = $(document.createElement('div'));
    helpBox.addClass('fieldhelp');

    var paras = helptext.split('\n');

    for ( var i = 0; i < paras.length; ++i )
	{
	    var hp = document.createElement('p');
	    $(hp).text(paras[i]);
	    helpBox.append(hp);
	}

    helpBox.hide();

    return(helpBox);
}

function _zingAddHelpTrigger(fld, helpbox)
{
    var trig = $(document.createElement('div'));
    trig.addClass('trigger');

    var i = $(document.createElement('img'));
    i.attr('alt', '?');
    i.attr('title', 'show/hide additional help for this field');
    trig.append(i);

    var showsrc = '/static/q.png';
    var hidesrc = '/static/nq.png';

    i.attr('src', showsrc);

    trig.click(function(e) {
		   $(helpbox).toggle();

		   if ($(helpbox).is(":visible"))
		       {
			   i.attr('src', hidesrc);
			   i.attr('alt', 'X');
			   trig.addClass('closetrigger');
		       }
		   else
		       {
			   i.attr('alt', '?');
			   i.attr('src', showsrc);

			   trig.removeClass('closetrigger');
		       }
	       });

    trig.insertBefore(fld);
}

function _buildZingForm(disable) {
    var ctr = $('#zingform');
    var url = sgZfBaseUrl+'template';

    if (!ctr) {
        return;
    }

    ctr.addClass('spinning');

    $.ajax(
    {
        url: url,
        dataType: 'json',

        error: function(xhr, textStatus, errorThrown) {
            ctr.removeClass('spinning');
        },

        success: function(data) {
            ctr.removeClass('spinning');

            zingForm(data, ctr, disable);


        }
    });
}

function _pullRectype(fn)
{
    var prefre = /^zing_(.+)/;

    var matches = fn.match(prefre);

    if (! matches)
        return(false);

    return(matches[1]);
}

function _zingPopulate(ctr, wit) {
    $('#zingform form')[0].reset();

    if (!wit) {
        return;
    }

    var rt = wit.rectype;
    var prefix = 'zing_' + rt + '_';

    for (var i in wit) {
        var fn = prefix + i;
        var fld = $('#' + fn);

        if ((!!fld) && (!! fld[0])) {
	    if (fld[0].localName == 'ul')
	    {
		fld.empty();

		for ( jj in wit[i] )
		{
		    var cmt = wit[i][jj];

		    var li = $(document.createElement('li'));
		    var tp = $(document.createElement('p'));
		    tp.text(cmt.text);
		    tp.addClass('cmttext');
		    li.append(tp);

		    var wwp = $(document.createElement('p'));
		    wwp.addClass('whowhen');
		    wwp.text(_niceUser(cmt.who) + ", " + shortDateTime(new Date(cmt.when / 1000)));
		    li.append(wwp);

		    fld.append(li);
		}

	    }
	    else
	    {
		fld.val(_scaleIn(i, wit[i]));
	    }

        }
    }

    var reporter = $('#' + prefix + 'reporter');

    if  ((!! reporter) && (reporter.val() == ''))
        reporter.val(sgUserName);
}

function _showChangesets(recid)
{
    var ctr = $('#zing_vc_changeset');

    $('#zing_vc_changeset ul').remove();

    if (! recid)
	return;

    var url = '/repos/' + startRepo + '/wit/records/' + recid + "/changesets";

    $.ajax(
	{
            "url": url,
	    "dataType": 'json',

	    "error": function(xhr, textStatus, errorThrown) {
		alert("err: " + textStatus);
	    },

	    "success": function(changesets) {
		if (changesets.length > 0)
		{
		    var u = $(document.createElement('ul'));

		    for ( var i in changesets ) {
			var cs = changesets[i];
			var csid = cs.commit;

			var desc = _sets[csid].desc;

			var li = $(document.createElement('li'));
			li.text(desc + " ");

			_addremover(li, cs, recid);


			u.append(li);
		    }

		    ctr.append(u);
		}
	    }

	}
    );
}

function _addremover(li, cs, bugrecid) {

    var a = $(document.createElement('a'));

    a.text('[-]');
    li.append(a);
    a.attr('href', '#');
    a.attr('title', 'remove this association');

    a.click( function(e)
	     {
		 _removeAssociation(cs.recid, bugrecid);
	     }
	   );
}

function _addComments(witno, repop)
{
    if ((! _wits[witno]) || (! _wits[witno].comments))
	return;

    for ( var i in _wits[witno].comments )
    {
	var cmtid = _wits[witno].comments[i];
	_wits[witno].comments[i] = _comments[cmtid];
    }
}


function _selectRecord(i, wit) {
    _zingPopulate($('#zingform'), wit);

    var recid = '';
    if (i >= 0)
	recid = wit.recid;

    $('#zfrecid').val(recid);

    $('#zingform input').removeAttr('disabled');
    $('#zingform select').removeAttr('disabled');

    $('#zingselection').hide();
    $('#newwi').hide();
            $('#wisearch').hide();
    $('#zingform').show();
    $('#backtolist').show();

    var title = '';

    if (i < 0)
    {
	$('#zfadditem').show();
	$('#zfupdateitem').hide();
        title = 'new work item';
    }
    else
    {
	$('#zfadditem').hide();
	$('#zfupdateitem').show();

        title = wit.id;

        if (!! wit.title)
            title += ": " + wit.title;
    }

    $('#wititle').text(title);

    _showChangesets(recid);


}

function _niceUser(uid)
{
    if ((!! _wiusers[uid]) && (_wiusers[uid] != ''))
        return(_wiusers[uid]);
    return(uid);
}
function _niceSprint(sid)
{
    if ((!! _sprints[sid]) && (_sprints[sid] != ''))
        return(_sprints[sid]);
    return(sid);
}

function _addWorkItemToList(ctr, i, wit)
{
    var title = wit.title || '';
    var assignee = _niceUser(wit.assignee || '');
    var status = wit.status || '';
    var reporter = _niceUser(wit.reporter || '');
    var verifier = _niceUser(wit.verifier || '');
    var sprint = _niceSprint(wit.sprint) || '';
    var recid = wit.recid || '';

    var clicker = function(e) {

        e.stopPropagation();
        e.preventDefault();

        _selectRecord(i, wit);
    };

    var row = $(document.createElement('tr')).click(clicker);

    row.append(
        $(document.createElement('td')).text(title).click(clicker)
    );
    row.append(
        $(document.createElement('td')).text(status).click(clicker)
    );
    row.append(
        $(document.createElement('td')).text(assignee).click(clicker)
    );
    row.append(
        $(document.createElement('td')).text(reporter).click(clicker)
    );
    row.append(
        $(document.createElement('td')).text(verifier).click(clicker)
    );
    row.append(
        $(document.createElement('td')).text(sprint).click(clicker).attr('id', recid + "_sprint")
    );

    ctr.append(row);
}

function _filterRowsOn(selectorignored, colno, rows)
{
    for ( var i = 0; i < rows.length; ++i )
    {
        var show = true;

	for ( var selid in _filters )
	{
	    var selector = $('#' + selid);

	    var v = selector.val();

            if ((!! v) && (v != ''))
            {
		var cols = $(rows[i]).children('td');
		colno = _filters[selid];

		if (cols.length > colno)
		{
                    var col = $(cols[colno]);
                    var cv = col.text();

                    show = (cv == v);

		    if (! show)
			break;
		}
            }
	}

        if (show)
            $(rows[i]).show();
        else
            $(rows[i]).hide();
    }
}

function _rememberSelection(selector)
{
    var cname = "sg" + selector.attr('id');
    var cval = selector.val();
    
    _setCookie(cname, cval, 30);
}


function _recallSelection(selector)
{
    var cname = "sg" + selector.attr('id');

    var cval = _findCookie(cname);

    if (cval)
    {
	selector.val(cval);
	selector.change();
    }
}

function _setupColFilter(selector, colno, rows)
{
    // clear out old list
    var opts = selector.children('option');

    for ( var  i = opts.length - 1; i > 0; --i )
        $(opts[i]).remove();

    var vals = {};

    if (selector.attr('id') == 'filter_sprint')
    {
        for ( i in _sprints )
            vals[_sprints[i]] = true;
    }
    else
    {
	for ( i = 1; i < rows.length; ++i )
	{
            var cols = $(rows[i]).children('td');

            if (cols.length > colno)
            {
		var col = $(cols[colno]);
		var v = col.text();
		
		if (v && (v != ''))
                    vals[v] = true;
            }
	}
    }

    var any = false;

    for ( var v in vals )
    {
        var opt = $(document.createElement('option')).val(v).text(v);
        selector.append(opt);
        any = true;
    }

    if (any)
        selector.removeAttr('disabled');
    else
        selector.attr('disabled', 'disabled');

    selector.change(
        function(e) {
	    _rememberSelection(selector);
            _filterRowsOn(selector, colno, rows);
        }
    );

    _recallSelection(selector);
}

function _setupFilters(headers, rows) {
    var re = /^filter_(.*)$/;

    for ( var i = 0; i < headers.length; ++i )
    {
        var header = $(headers[i]);
        var sel = header.children('select');
        var matches = false;

        if ((!! sel) && (sel.length == 1))
            matches = sel.attr('id').match(re);

        if (matches)
        {
	    _filters[sel.attr('id')] = i;
            _setupColFilter(sel, i, rows);
        }
    }
}

function _addAdder()
{
    var adder = $('#newwi');

    adder.click(
        function(e) {
            e.stopPropagation();
            e.preventDefault();

            _selectRecord(-1, { 'title': '(add a new work item)', 'rectype': 'item' } );
        }
    );
}

function _refreshWiList(ctr)
{
    ctr.empty();

    for (var i in _wits) {
        var wit = _wits[i];

        _addWorkItemToList(ctr, i, _wits[i]);
    }

    _setupFilters($('#zingselection th'), $('#zinglist tr'));
}

function _zingListWorkItems() {
    var ctr = $('#zinglist');
    var lurl = sgZfBaseUrl+'records';

    if (!ctr) {
        return;
    }

    var v = $('#wisearchtext').val();
    if ((!! v) && (v != ''))
        lurl = lurl + '?q=' + encodeURIComponent(v);

    $('#zingform').hide();

    ctr.addClass('spinning');

    $.ajax(
    {
        url: lurl,
        dataType: 'json',

        error: function(xhr, textStatus, errorThrown) {
            ctr.removeClass('spinning');
        },

        success: function(wits) {
            ctr.removeClass('spinning');
	    _wits = wits;

            for ( var j = _wits.length - 1; j >= 0; --j )
            {
	        if (_wits[j].rectype == 'sprint')
                    _sprints[_wits[j].recid] = _wits[j].name;
	        if (_wits[j].rectype == 'comment')
                    _comments[_wits[j].recid] = _wits[j];
	        if (_wits[j].rectype != 'item')
	            _wits.splice(j, 1);
            }

            _refreshWiList(ctr);

            for (var i in _wits) {
                var wit = _wits[i];

	        _addComments(i);

            }

            $("#zinglist tr").hover(
	        function() {
                    $(this).addClass('highlight');
	        },
                function() {
                    $(this).removeClass('highlight');
                });

            var re = /\/records\/([a-g0-9]+)\/?$/;

            var matches = window.location.href.match(re);

            if (matches) {
                var trecid = matches[1];

                for (var i = 0; i < _wits.length; ++i) {
                    if (_wits[i].recid == trecid) {
                        _selectRecord(i, _wits[i]);
                    }
                }
            }


        }
    });

    $('#backtolist').hide();

    $('#backtolist').click(
        function(e) {

            e.stopPropagation();
            e.preventDefault();

            $('#backtolist').hide();
            $('#zingform').hide();
            $('#zingselection').show();
            $('#newwi').show();
            $('#wisearch').show();
            $('#wititle').text('work items');

            if (_wiListDirty)
            {
                _wiListDirty = false;
                _refreshWiList(ctr);
            }
        }
    );

}

function _reportError(url, xhr, textStatus, errorThrown) {
    var msg = "error on " + url + ":\r\n" + textStatus;

    if (!! errorThrown)
        msg += "\r\n" + errorThrown;

    if (!!xhr.responseText)
        msg += "\r\n" + xhr.responseText;

    alert(msg);
}


function _wireUpSearchBox()
{
    $('#wisearch').submit(
        function (e) {
            e.stopPropagation();
            e.preventDefault();

            _zingListWorkItems();
        }
    );
}


function sgKickoff() {
    _getUserList();
    _addAdder();
    _wireUpSearchBox();
}
