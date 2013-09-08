/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.novinet.com/license

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

trace = arbor.etc.trace;
objmerge = arbor.etc.objmerge;
objcopy = arbor.etc.objcopy;
var parse = Parser().parse;
var mainCanvas = arbor.ParticleSystem(50, 512, 0.5);

function setContent(text) {
  var network = parse(text);
  $.each(network.nodes, function (nname, ndata) {
    if (ndata.label === undefined) ndata.label = nname;
  });
  mainCanvas.merge(network);
  mainCanvas.renderer.redraw();
}

function GetClosestNode(e) {
  var pos = $("#mainViewport").offset();
  var mouseP = arbor.Point(e.pageX - pos.left, e.pageY - pos.top);
  return mainCanvas.nearest(mouseP);
}

function IsParentNode(node) {
  return node.data.routingNodeType == "mainNode" || node.data.routingNodeType == "dataNode";
}

function UpdateCanvasOnContainerResize() {
  var newHeight = $(window).height();
  if ($("#container").is(':visible'))
    newHeight = newHeight - 120;
  var newWidth = $(window).width();
  var canvas = $("#mainViewport").get(0);
  canvas.width = newWidth;
  canvas.height = newHeight;
  mainCanvas.screenSize(newWidth, newHeight);
  mainCanvas.renderer.redraw();
}

$(document).on("click", "#mainViewport", function (e) {
  var that = this;
  setTimeout(function () {
    var dblclick = parseInt($(that).data('double'), 10);
    if (dblclick > 0) {
      $(that).data('double', dblclick - 1);
    } else {
      singleClick.call(that, e);
    }
  }, 300);
  return false;
});

function singleClick(e) {
  $("#hide_button").click();
  if (IsParentNode(GetClosestNode(e).node))
    return false;
  var nodeName = GetClosestNode(e).node.name.toString();
  alert('click-' + nodeName);
}

$(document).on("dblclick", "#mainViewport", function (e) {
  $(this).data('double', 2);
  doubleClick.call(this, e);
  return false;
});

function doubleClick(e) {
  $("#hide_button").click();
  if (IsParentNode(GetClosestNode(e).node))
    return false;
  var nodeName = GetClosestNode(e).node.name.toString();
  alert('dblclick-' + nodeName);
}

$(document).on("contextmenu", "#mainViewport", function (e) {
  $('#node_name').val(GetClosestNode(e).node.name.toString());
  var routingDistance = GetClosestNode(e).node.data.routingDistance || "";
  $('#node_distance').val(routingDistance);
  $("#container").show();
  UpdateCanvasOnContainerResize();
  UpdateCanvasOnContainerResize();  // Re-render Required to fill Scrollbar region
  return false;
});

$(document).ready(function () {
  mainCanvas.renderer = Renderer("#mainViewport");
  setContent('');
  $("#container").hide();
  UpdateCanvasOnContainerResize();

  $(window).resize(function () {
    UpdateCanvasOnContainerResize();
  });
  
  $("#hide_button").click(function () {
    $("#container").hide();
    UpdateCanvasOnContainerResize();
  });

  $("#new_window_button").click(function () {
    alert('newview-' + $('#node_name').val());
  });
});
