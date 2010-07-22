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

function getStatus() {

    var ctr = $('#maincontent');
    ctr.addClass('spinning');

    $.ajax(
    {
        url: "/local/status",
        dataType: 'json',
        cache: false,

        error: function(xhr, textStatus, errorThrown) {
            ctr.removeClass('spinning');
        },

        success: function(data) {
            ctr.removeClass('spinning');
           
            var i = 0;
            var dl = $(document.createElement("dl"));
            $.each(data,
			  function(index, value) {
			      i++;
			      var dt = $(document.createElement("dt"));
			      dt.text(index);
			      dl.append(dt);

			      var dd = $(document.createElement("dd"));
			      dl.append(dd);

			      var ul = $(document.createElement("ul"));
			      dd.append(ul);

			      $.each(value, function(ii, vv) {
			          var li = $(document.createElement("li"));
			          if (index == "Modified") {
			              var diffURL = findBaseURL("status") + "/repo/diff/" + vv.old_hid + "/" + vv.path;
			              var a_diff = $('<a></a>').text(vv.path).attr({ href: "#", 'title': "diff against current working directory version" })
			                                        .addClass("download_link").click(function(e) {
			                                            e.preventDefault();
			                                            e.stopPropagation();
			                                            showDiff(diffURL, $('<div></div>'), true);
			                                        });
			              li.html(a_diff);
			          }
			          else
			              li.text(vv.path);

			          ul.append(li);
			      }
				    );
			  }
			 );
			 if (i==0) {
                var div = $('<p>').width(300).height(30);
                div.text("no changes in your working directory").addClass("highlight");

                ctr.append(div);
                return;

            }

            ctr.append(dl);
        }
    });
}


function sgKickoff() {
    getStatus();
}
