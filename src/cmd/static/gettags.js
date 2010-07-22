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
var tagItems = [];
var scrollPosition = 0;

function clearFilter() {

    currentNumber = 0;
    $('#maincontent').addClass('spinning');
    $('#tag').val("");
    $('#tabletags tr').remove();
    showTags();
    $('#maincontent').removeClass('spinning');
}

function filterTags(text) {
    currentNumber = 0;
    $('#maincontent').addClass('spinning');
    var filtered = [];
    $('#tabletags tr').remove();

    $.each(tagItems, function(i, v) {
        if (v.tag.indexOf(text) >= 0)
            filtered.push(v);

    });
   
    showTags(filtered);
    $('#maincontent').removeClass('spinning');

}

function showTags(items) {

    var show = items || tagItems;

    if (show.length > currentNumber) {
        var showItems = show.slice(currentNumber, currentNumber += dataChunk);

        $.each(showItems,
			     function(index, value) {
			         var tr = $('<tr>');

			         var td1 = $('<td>');
			         var td2 = $('<td>');

			         td1.text(value.tag);

			         var urlBase  = window.location.pathname.rtrim("/").rtrim("tags").rtrim("/");
			         var url = urlBase + "/changesets/" + value.csid;
			         var csa = $('<a>').text(value.csid).attr({ href: url, 'title': "view changeset"});
			         td2.append(csa);


			         tr.append(td1);
			         tr.append(td2);

			         $('#tabletags').append(tr);

			     });



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

function getTags() {
    $(window).scrollTop(scrollPosition);

    $('#maincontent').css("height", "300px");
    $('#maincontent').addClass('spinning');

    var str = "";


    var url = window.location.pathname + "/tags/";

    $.ajax({
        url: url,
        dataType: 'json',
        cache: false,
        error: function(data, textStatus, errorThrown) {
            $('#maincontent').removeClass('spinning');
            reportError(null, xhr, textStatus, errorThrown);
        },
        success: function(data, status, xhr) {
            $('#maincontent').removeClass('spinning');
            $('#maincontent').css("height", "");
            if ((status != "success") || (!data)) {
                return;
            }
            tagItems = data.sort(function(a, b) {
                return (a.tag.toLowerCase().compare(b.tag.toLowerCase()));
                
            }); 
            showTags();
            $(window).scroll(scrollPosition);
        }
    });

}


function sgKickoff() {

    getTags();
    $('#tag').keyup(function() {
        var text = $('#tag').val();
        if (text.length > 0) {
            filterTags(text);
        }
        else {
            clearFilter();
        }

    });
    
 

    $(window).scroll(function() {

        scrollPosition = $(window).scrollTop();
        if (isScrollBottom()) {
            showTags();
        }
    });

    

}