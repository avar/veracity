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

function DagNode(v)
{
    this.value = v;
    this.pos = -1;
    this.isdummy = false;
    this.minup = 0;
    this.parents = [];
    this.kids = [];
    this.hcenter = -1;
    this.vcenter = -1;
    this.desc = "";
}

DagNode.prototype.addParent = function(parentNode) {
    this.parents.push(parentNode);
    parentNode.kids.push(this);
};

DagNode.prototype.upLevel = function() {
    var i = 0;

    for ( i = 0; i < this.parents.length; ++i )
    {
	if (this.parents[i].minup <= this.minup)
	{
	    this.parents[i].minup = this.minup + 1;
	    this.parents[i].upLevel();
	}
    }
};


var svgns = "http://www.w3.org/2000/svg";

function replaceNode(nodelist, node, newNode) {
    for ( var nnn = 0; nnn < nodelist.length; ++nnn )
    {
	if (nodelist[nnn] == node)
	{
	    nodelist[nnn] = newNode;
	    break;

	}
    }

    return(nodelist);
}

    function svgpoint(x, y)
    {
	return("" + x + "," + y + " ");
    }

    function avg(a, b)
    {
	return( (a + b) / 2 );
    }

function _addClick(svg, obj, node) {
    if (! node.isdummy)
    {
	$(obj).attr("cursor", "pointer");

	$(obj).click(
	    function(e) {
		var clickurl = "/repo/" + startRepo + "/changesets/" +
		    node.value;

		e.stopPropagation();
		e.preventDefault();

		window.document.location.href = clickurl;
	    }
	);
    }
}

function drawIt(container, svg) {
    var doc = window.document;
    var right = 0;
    var bottom = 0;
    var _sgScale = 1.0;
    var _sgVpXy = [0,0];


//    var c = doc.createElementNS(svgns, "circle");

    var nodes = [];
    var nodedict = {};

    for ( var i in relations ) {

	var parents = relations[i].parents;
	var node = false;
	var value = relations[i].changeset_id;

	if (nodedict[value])
	{
	    node = nodedict[value];

	}
	else
	{
	    node = new DagNode(value);
	    nodedict[value] = node;
	    nodes.push(node);
	}

	node.label = "";

	node.desc = "";

	if (relations[i].comments.length > 0)
	{
	    node.desc = relations[i].comments[0].text;
	    node.label = node.desc.substring(0, 20);
	    if (node.desc.length > 20)
		node.label += "...";
	    node.label += "\n";
	}

	if (relations[i].audits.length > 0)
	    node.label += _shortWho(relations[i].audits[0].who)   + "\n" + shortDateTime(new Date(relations[i].audits[0].when / 1000));

	if (relations[i].tags.length > 0)
	    node.label += "\r\n (" + relations[i].tags.join(',') + ")";

	var parent = false;

	for ( var pn = 0; pn < parents.length; ++pn )
	{
	    if (nodedict[parents[pn]])
	    {
		parent = nodedict[parents[pn]];
	    }
	    else
	    {
		parent = new DagNode(parents[pn]);
		nodedict[parents[pn]] = parent;
		nodes.push(parent);
	    }

	    node.addParent(parent);
	}

    }

    var nn;
    for ( nn = 0; nn < nodes.length; ++nn )
    {
	nodes[nn].upLevel();
    }

    for ( nn = 0; nn < nodes.length; ++nn )
    {
	var node = nodes[nn];

	if ((node.kids.length == 0) && (node.parents.length > 0))
	{
	    var movable = true;

	    while (movable)
	    {
		for ( var ppn = 0; ppn < node.parents.length; ++ppn )
		{
		    if ((node.parents[ppn].minup - node.minup) <= 1)
		    {
			movable = false;
			break;
		    }
		}

		if (movable)
		{
		    ++node.minup;
		}
	    }
	}

	for ( var pn = 0; pn < node.parents.length; ++pn )
	{
	    var parent = node.parents[pn];

	    if ((parent.minup - node.minup) > 1)
	    {
		var dummy = new DagNode("dummy");

		replaceNode(node.parents, parent, dummy);
		replaceNode(parent.kids, node, dummy);
		dummy.parents = [ parent ];
		dummy.kids = [ node ];

		dummy.minup = node.minup + 1;
		dummy.isdummy = true;
		nodes.push(dummy);
	    }
	}
    }

    var layers = [];

    for ( nn = 0; nn < nodes.length; ++nn )
    {
	var vl = nodes[nn];

	while (vl.minup >= layers.length) {
	    layers.push([]);
	}

	layers[vl.minup].push(vl);
    }

    layers.reverse();

    for ( var iter = 0; iter < 3; ++iter )
    {
	reduceCrossings(layers);
    }

    for ( lno = 0; lno < layers.length; ++lno )
    {
	layers[lno] = layoutLayer(layers[lno], false);
    }
    for ( lno = layers.length - 1; lno >= 0; --lno )
    {
	layers[lno] = layoutLayer(layers[lno], true);
    }

    normalizeLayout(nodes);

    for ( lno = 0; lno < layers.length; ++lno )
    {
	var layer = layers[lno];
	var hcenter = 35;
	var vcenter = 35 + (75 * lno);

	for ( var nnode = 0; nnode < layer.length; ++nnode )
	{
	    var node = layer[nnode];

	    hcenter = 35 + (node.pos * 75);

	    node.hcenter = hcenter;
	    node.vcenter = vcenter;

	    var radius = 25;
	    var cstroke = "#F0F0F0";
	    var cfill = "#F6F6F6";

	    var drawDummies = false;

	    if (node.isdummy) {
		cstroke = "red";
		radius = 5;
	    }

	    if (drawDummies || ! node.isdummy)
	    {
		// todo : does this return an element?
		var circle = svg.circle(null, hcenter, vcenter, radius,  {
			       fill: cfill, stroke: cstroke
			   });

		_addClick(svg, circle, node);

// todo:		_insertAtTop(sdoc, circle);

		bottom = Math.max(bottom, vcenter + radius);
		right = Math.max(right, hcenter + radius);

		var lineHeight = 12;

		var lines = node.label.split("\n");
		var blockHeight = lineHeight * lines.length;
		var initY = vcenter - Math.floor((blockHeight / 2) - (lineHeight / 2));

		for ( var lineno = 0; lineno < lines.length; ++lineno )
		{
		    var line = lines[lineno];

		    var text = svg.text(null, hcenter, initY, line,
					{
					    "font-size": "8px",
					    "fill": "#000",
					    "text-anchor": "middle",
					    "pointer-events": "none"
					}
				       );

// todo:		    _insertAfter(sdoc, text, circle);

		    var sib = circle.parentNode.firstChild;

		    if (sib && (sib != text))
			$(sib).before(text);

		    _addClick(svg, text, node);

		    initY += lineHeight;
		}

		var sib = circle.parentNode.firstChild;

		if (sib && (sib != circle))
		    $(sib).before(circle);

		if ((!! node.desc) && (node.desc != ''))
		{
		    var g = svg.group(null, null, { visibility: "hidden" });
		    var text = svg.text(g, hcenter + (radius * 2), vcenter + radius, node.desc,
			{
			    "font-size": "10px",
			    "fill": "#000",
			    "text-anchor": "left"
			}
				       );

		    var textHeight = 10;
		    var textWidth = text.getComputedTextLength();
		    var textLeft = text.offsetLeft;
		    var textTop = text.offsetTop - textHeight;



		    var bounder = svg.rect(g, textLeft - 6, textTop - 6, textWidth + 12, textHeight + 12, 4, 4, { fill: "#F0F0FF", "stroke" : "#FFF", "stroke-width" : 2 } );
		    $(text).before(bounder);

		    g.setAttributeNS(null, "visibility", "hidden");

		    _addClick(svg, text, node);

		    addMouseover(circle, g);

		}
	    }
	}
    }

    var maxCenter = 0;

    for ( nn = 0; nn < nodes.length; ++nn )
    {
	var node = nodes[nn];

	if (node.hcenter > maxCenter)
	{
	    maxCenter = node.hcenter;
	}

	if (! node.isdummy)
	{
	    for ( var nk = 0; nk < node.kids.length; ++nk )
	    {
		var kid = node.kids[nk];

		if (kid.isdummy)
		{
		    var path = "M" + svgpoint(node.hcenter, node.vcenter + 25);
		    path += "C" + svgpoint(avg(kid.hcenter, node.hcenter), node.vcenter + 25);
		    path += svgpoint(kid.hcenter, avg(kid.vcenter, node.vcenter));
		    path += svgpoint(kid.hcenter, kid.vcenter);

		    while (kid.isdummy)
		    {
			newkid = kid.kids[0];

			path += "S" + svgpoint(newkid.hcenter, avg(newkid.vcenter, kid.vcenter));

			if (newkid.isdummy)
			{
			    path += svgpoint(newkid.hcenter, newkid.vcenter);
			}
			else
			{
			    path += svgpoint(newkid.hcenter, newkid.vcenter - 25);
			}

			kid = newkid;
		    }

		    svg.path(path,
			     {
				 fill: "none",
				 "stroke": "black",
			     }
			    );
		}
		else
		{
		    var kradius = 25;
		    var nradius = 25;

		    svg.line(node.hcenter, node.vcenter + nradius, kid.hcenter, kid.vcenter - kradius,
			     {
				 "stroke": "black"
			     }
			    );
		}
	    }
	}
}

right += 200;

    var cheight = $(document).height() - 20;
    var cwidth = $(window).width() - 20;

    container.style.height = cheight + 'px';
    container.style.width = cwidth + 'px';

    if (bottom < cheight - 20)
	bottom = cheight - 20;
    if (right < cwidth - 20)
	right = cwidth - 20;

    var ctrHeight = bottom - 20;
    var ctrWidth = right - 20;

    svg.root().setAttributeNS(null, "height", bottom + 10);
    svg.root().setAttributeNS(null, "width",  right);

    $(container).click(
	function(e) {
	    var zoomOut = ((!! e.which) && (e.which == 3)) ||
		((!! e.ctrlKey) && e.ctrlKey);
	    var ht = $(svg.root()).attr("height");
	    var wd = $(svg.root()).attr("width");

	    var ctry = e.offsetY || e.clientY;
	    var ctrx = e.offsetX || e.clientX;

	    ctrx = (ctrx / _sgScale) + _sgVpXy[0];
	    ctry = (ctry / _sgScale) + _sgVpXy[1];

	    if (zoomOut)
		_sgScale = _sgScale / 0.75;
	    else
		_sgScale = _sgScale * 0.75;

	    var scaledCtrHeight = ctrHeight * _sgScale;
	    var scaledCtrWidth = ctrWidth * _sgScale;

	    var scaledLeft = Math.floor((ctrx * _sgScale) - (scaledCtrWidth / 2.0));
	    var scaledTop = Math.floor((ctry * _sgScale) - (scaledCtrHeight / 2.0));
	    var scaledWidth = wd * _sgScale;
	    var scaledHeight = ht * _sgScale;

	    if (scaledLeft > 0)
	    {
		_debugOut("scaledLeft: " + scaledLeft);
		if ((scaledWidth - scaledLeft) < scaledCtrWidth)
		    scaledLeft -= (scaledCtrWidth - (scaledWidth - scaledLeft));
		_debugOut("scaledLeft: " + scaledLeft);
	    }
	    if (scaledTop > 0)
	    {
		_debugOut("scaledTop: " + scaledTop);
		if ((scaledHeight - scaledTop) < scaledCtrHeight)
		    scaledTop -= (scaledCtrHeight - (scaledHeight - scaledTop));
		_debugOut("scaledTop: " + scaledTop);
	    }

	    if (scaledLeft < 0)
		scaledLeft = 0;
	    if (scaledTop < 0)
		scaledTop = 0;

	    _sgVpXy[0] = scaledLeft;
	    _sgVpXy[1] = scaledTop;

	    var vps = _sgVpXy[0] + " " +
		_sgVpXy[1] + "  " +
		scaledCtrWidth + " " + 
		scaledCtrHeight;

//	    var params = {};
//	    params["svgViewBox"] = vps;

//	    $(svg.root()).animate(params, 1000);
	    $(svg.root()).attr("viewBox", vps);
	}
    );

    return(svg);
}


function _insertAtTop(sdoc, node)
{

    sdoc.insertBefore(node, sdoc.firstChild);
}

function _insertAfter(sdoc, newNode, oldNode)
{
    if (!! oldNode.nextSibling)
	sdoc.insertBefore(newNode, oldNode.nextSibling);
    else
	sdoc.appendChild(newNode);
}

function indexIn(layer, node)
{
    for ( var nn = 0; nn < layer.length; ++nn )
    {
	if (layer[nn] == node)
	{
	    return(nn);
	}
    }

    return(-1);
}

function swap(layer, a, b)
{
    var t = layer[a];
    layer[a] = layer[b];
    layer[b] = t;
}

function reduceCrossings(layers)
{
    for (var lno = 1; lno < layers.length; ++lno)
    {
	var layer = layers[lno];
	var above = layers[lno - 1];

	var means = [];

	for (var nno = 0; nno < layer.length; ++nno)
	{
	    var node = layer[nno];

	    var tots = 0.0;

	    for (var pno = 0; pno < node.parents.length; ++pno)
	    {
		tots += indexIn(above, node.parents[pno]);
	    }

	    if (node.parents.length > 0)
	    {
		node.mean = tots / node.parents.length;
	    }
	    else
	    {
		node.mean = nno + 0.0;
	    }
	}

    }

    for (lno = layers.length - 2; lno >= 0; --lno)
    {
	layer = layers[lno];
	below = layers[lno + 1];

	means = [];

	for (var nno = 0; nno < layer.length; ++nno)
	{
	    var node = layer[nno];

	    var tots = 0.0;

	    for (var pno = 0; pno < node.kids.length; ++pno)
	    {
		tots += indexIn(below, node.kids[pno]);
	    }

	    if (node.kids.length > 0)
	    {
		node.mean = tots / node.kids.length;
	    }
	    else
	    {
		node.mean = nno + 0.0;
	    }
	}


    }

}

function normalizeLayout(nodes)
{
    var minpos = 0;

    for ( var i = 0; i < nodes.length; ++i )
    {
	if (nodes[i].pos < minpos)
	{
	    minpos = nodes[i].pos;
	}
    }

    var offset = 0 - minpos;

    for ( var i = 0; i < nodes.length; ++i )
    {
	nodes[i].pos += offset;
    }

}

function medianParent(node)
{
    return(medianRelative(node, node.parents));
}

function medianChild(node)
{
    return(medianRelative(node, node.kids));
}

function medianRelative(node, relatives)
{
    var ppos = [];

    for ( var i = 0; i < relatives.length; ++i )
    {
	ppos.push(relatives[i]);
    }

    ppos.sort(function(a,b) { return a.pos - b.pos });

    if (ppos.length > 0)
    {
	return(ppos[Math.min(Math.round(ppos.length / 2), ppos.length - 1)].pos);
    }
    else
    {
	return(node.pos);
    }

}

/**
 * can't count on Array.sort() being stable across JS engines; really need it to be for layout
 */
function layerSort(arr) {

  // return if array is unsortable
  if (arr.length <= 1){
    return arr;
  }

  var less = Array(), greater = Array();

  // select and remove a pivot value pivot from array
  // a pivot value closer to median of the dataset may result in better performance
  var pivotIndex = Math.floor(arr.length / 2);
  var pivot = arr.splice(pivotIndex, 1)[0];

  // step through all array elements
  for (var x = 0; x < arr.length; x++){

    // if (current value is less than pivot),
    // OR if (current value is the same as pivot AND this index is less than the index of the pivot in the original array)
    // then push onto end of less array
    if (
	(arr[x].pos < pivot.pos)
      ||
      (arr[x].pos == pivot.pos && x < pivotIndex)  // this maintains the original order of values equal to the pivot
    ){
      less.push(arr[x]);
    }

    // if (current value is greater than pivot),
    // OR if (current value is the same as pivot AND this index is greater than or equal to the index of the pivot in the original array)
    // then push onto end of greater array
    else {
      greater.push(arr[x]);
    }
  }

  // concatenate less+pivot+greater arrays
  return layerSort(less).concat([pivot], layerSort(greater));
}


  function layoutLayer(layer, up)
{
    var lastpos = -1;
    var pos = [];
    var lleft = false;

    for ( var j = 0; j < layer.length; ++j )
    {
	var node = layer[j];
	if (node.pos < 0)
	    {
		node.pos = j;
	    }

	if (up)
	    node.pos = medianChild(node);
	else
	    node.pos = medianParent(node);
    }

    layer = layerSort(layer);
//    layer.sort(function(a,b) { return a.pos - b.pos });


    var conflicts = 0;
    var nconflicts = 0;

    do
    {
	conflicts = 0;
	++nconflicts;

	if (nconflicts > 10)
	{
//	    debugger;
	    ++nconflicts;
	}

	if (up)
	{
	    for ( j = layer.length - 1; j > 0; --j )
	    {
		if (layer[j].pos == layer[j - 1].pos)
		{
		    if (layer[j].isdummy)
		    {
			layer[j - 1].pos--;
		    }
		    else if (layer[j - 1].isdummy)
		    {
			layer[j].pos--;
		    }
		    else
		    {
			layer[j - 1].pos--;

		    }

		    ++conflicts;
		}
	    }
	}
	else
	{
	    for ( j = 1; j < layer.length; ++j )
	    {
		if (layer[j].pos == layer[j - 1].pos)
		{
		    if (layer[j - 1].isdummy)
		    {
			layer[j].pos++;
		    }
		    else if (layer[j].isdummy)
		    {
			layer[j - 1].pos++;
		    }
		    else
		    {
			layer[j].pos++;

		    }

		    ++conflicts;
		}
	    }
	}

	if (conflicts > 0)
	{
	    layer = layerSort(layer);
	}


    } while (conflicts > 0);

    return(layer);
}


function addMouseover(circle, text) {

    circle.onmouseover = function(e) {
        text.setAttributeNS(null, "visibility", "visible");
    };
    circle.onmouseout = function(e) {
        text.setAttributeNS(null, "visibility", "hidden");
    };

}
