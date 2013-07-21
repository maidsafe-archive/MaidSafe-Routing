(function() {

  Renderer = function(canvas) {
    var canvas = $(canvas).get(0)
    var ctx = canvas.getContext("2d");
    var gfx = arbor.Graphics(canvas)
    var particleSystem = null

    var that = {
      init: function(system) {
        particleSystem = system
        particleSystem.screenSize(canvas.width, canvas.height)
        particleSystem.screenPadding(40)

        that.initMouseHandling()
      },

      drawRoundedRect: function(x, y, width, height, fill, parentNode, isExpanded) {
        var radius = 5;
        var strokeWidth = isExpanded ? 6 : 2;
        var strokeStyle = parentNode ? "rgb(20,20,20)" : "rgb(0,205,0)";
        ctx.save();
        ctx.fillStyle = fill;
        ctx.strokeStyle = strokeStyle;
        ctx.beginPath();
        if (parentNode || isExpanded)
          ctx.lineWidth = strokeWidth;
        ctx.moveTo(x + radius, y);
        ctx.lineTo(x + width - radius, y);
        ctx.quadraticCurveTo(x + width, y, x + width, y + radius);
        ctx.lineTo(x + width, y + height - radius);
        ctx.quadraticCurveTo(x + width, y + height, x + width - radius, y + height);
        ctx.lineTo(x + radius, y + height);
        ctx.quadraticCurveTo(x, y + height, x, y + height - radius);
        ctx.lineTo(x, y + radius);
        ctx.quadraticCurveTo(x, y, x + radius, y);
        ctx.closePath();
        if (parentNode || isExpanded)
          ctx.stroke(true);
        ctx.fill(true);
        ctx.restore();
      },

      drawDashedLine: function(x, y, x2, y2, dashArray) {
        if (!dashArray) dashArray = [10, 5];
        var dashCount = dashArray.length;
        ctx.moveTo(x, y);
        var dx = (x2 - x), dy = (y2 - y);
        var slope = dy / dx;
        var distRemaining = Math.sqrt(dx * dx + dy * dy);
        var dashIndex = 0, draw = true;
        while (distRemaining >= 0.1) {
          var dashLength = dashArray[dashIndex++ % dashCount];
          if (dashLength > distRemaining) dashLength = distRemaining;
          var xStep = Math.sqrt(dashLength * dashLength / (1 + slope * slope));
          if (dx < 0) xStep = -xStep;
          x += xStep;
          y += slope * xStep;
          ctx[draw ? 'lineTo' : 'moveTo'](x, y);
          distRemaining -= dashLength;
          draw = !draw;
        }
      },

      redraw: function() {
        if (!particleSystem) return

        gfx.clear() // convenience ƒ: clears the whole canvas rect

        // draw the nodes & save their bounds for edge drawing
        var nodeBoxes = {}
        particleSystem.eachNode(function(node, pt) {
          // node: {mass:#, p:{x,y}, name:"", data:{}}
          // pt:   {x:#, y:#}  node position in screen coords

          // determine the box size and round off the coords if we'll be 
          // drawing a text label (awful alignment jitter otherwise...)
          var label = node.data.label || "";
          var prefixNodeId = label.substr(0, 6) || "";
          var suffixNodeId = label.substr(label.length - 6, 6) || "";
          var proximityId = node.data.proximityId || "";
          var isExpanded = node.data.isExpanded == "1";
          var isParentNode = node.data.routingNodeType == "mainNode" || node.data.routingNodeType == "dataNode";

          var w = 10 + Math.max(
            Math.max(ctx.measureText("" + prefixNodeId).width, ctx.measureText("" + suffixNodeId).width),
            ctx.measureText("Prox: " + proximityId).width);

          var hMultiplier = 0;
          if (prefixNodeId != "")
            hMultiplier = hMultiplier + 1;
          if (suffixNodeId != "")
            hMultiplier = hMultiplier + 1;
          if (proximityId != "")
            hMultiplier = hMultiplier + 1;

          if (isParentNode) {
            w = 30 + ctx.measureText(prefixNodeId + "..." + suffixNodeId).width;
            hMultiplier = 3;
          }

          var h = 20 * hMultiplier;

          if (!("" + label).match(/^[ \t]*$/)) {
            pt.x = Math.floor(pt.x);
            pt.y = Math.floor(pt.y);
          } else {
            label = null;
          }
          // draw a rectangle centered at pt
          if (isParentNode) {
            ctx.fillStyle =
              node.data.routingNodeType == "mainNode" ? "rgb(205,0,0)" : "rgb(32,193,190)";
          } else if (proximityId != "") {
            ctx.fillStyle = "rgb(20,20,20)";
          } else {
            ctx.fillStyle = "rgb(140,140,140)";
          }

          node.data.shape = 'rect';
          that.drawRoundedRect(pt.x - w / 2, pt.y - h / 2, w, h, ctx.fillStyle, isParentNode, isExpanded);
          var strokeThickness = 4;
          nodeBoxes[node.name] = [pt.x - w / 2 - strokeThickness, pt.y - h / 2 - strokeThickness, w + strokeThickness * 2, h + strokeThickness * 2];


          /*if (node.data.shape == 'dot') {
            gfx.oval(pt.x - w / 2, pt.y - w / 2, w, w, { fill: ctx.fillStyle });
            nodeBoxes[node.name] = [pt.x - w / 2, pt.y - w / 2, w, w];
          } else {
            gfx.rect(pt.x - w / 2, pt.y - h / 2, w, h, 0, { fill: ctx.fillStyle });
          }*/

          // draw the text
          if (label) {
            ctx.font = "12px Courier";
            ctx.textAlign = "center";
            ctx.fillStyle = "white";
            if (node.data.color == 'none')
              ctx.fillStyle = '#333333';
            if (isParentNode) {
              ctx.font = "14px Courier";
              ctx.fillText(prefixNodeId + "..." + suffixNodeId, pt.x, pt.y + 5);
            } else if (proximityId != "") {
              ctx.fillText("Prox: " + proximityId, pt.x, pt.y - 12);
              ctx.fillText(prefixNodeId || "", pt.x, pt.y + 10);
              ctx.fillText(suffixNodeId || "", pt.x, pt.y + 24);
            } else {
              ctx.fillText(prefixNodeId || "", pt.x, pt.y - 2);
              ctx.fillText(suffixNodeId || "", pt.x, pt.y + 12);
            }
          }
        });


        // draw the edges
        particleSystem.eachEdge(function(edge, pt1, pt2) {
          // edge: {source:Node, target:Node, length:#, data:{}}
          // pt1:  {x:#, y:#}  source position in screen coords
          // pt2:  {x:#, y:#}  target position in screen coords

          //var weight = edge.data.weight
          var weight = 1;
          var arrowWeight = !isNaN(weight) ? parseFloat(weight) : 1;
          var arrowLength = 8 + arrowWeight;
          var arrowWidth = 6 + arrowWeight;
          var color = edge.data.color;

          if (!color || ("" + color).match(/^[ \t]*$/)) color = null;


          // find the start point
          var sourceBox = nodeBoxes[edge.source.name];
          var tail = intersect_line_box(pt1, pt2, nodeBoxes[edge.source.name]);
          if (edge.data.doubleArrow) {
            var morphedSourceBox = [sourceBox[0] - arrowWidth, sourceBox[1] - arrowWidth, sourceBox[2] + arrowWidth * 2, sourceBox[3] + arrowWidth * 2];
            tail = intersect_line_box(pt1, pt2, morphedSourceBox);
          }

          var head = intersect_line_box(tail, pt2, nodeBoxes[edge.target.name])

          if (!edge.data.onlyShowArrow) {
            ctx.save()
            ctx.beginPath()
            ctx.lineWidth = (!isNaN(weight)) ? parseFloat(weight) : 1;
            ctx.strokeStyle = (color) ? color : "rgb(120,120,120)";
            ctx.fillStyle = "rgb(255,255,255)";
            if (edge.data.doubleArrow) {
              //ctx.lineWidth = weight * 2;
              ctx.moveTo(tail.x, tail.y);
              ctx.lineTo(head.x, head.y);
            } else {
              that.drawDashedLine(tail.x, tail.y, head.x, head.y, [2, 4]);
            }
            ctx.stroke();
            ctx.restore();
          }

          // draw an arrowhead if this is a -> style edge
          if (edge.data.directed) {
            ctx.save();
            // move to the head position of the edge we just drew

            if (edge.data.nodeStatus == "0")
              ctx.fillStyle = "rgb(50,150,50)";
            else if (edge.data.nodeStatus == "1")
              ctx.fillStyle = "rgb(0,100,255)";
            else
              ctx.fillStyle = "rgb(250,150,50)";

            ctx.translate(head.x, head.y);
            ctx.rotate(Math.atan2(head.y - tail.y, head.x - tail.x));

            // delete some of the edge that's already there (so the point isn't hidden)
            ctx.clearRect(-arrowLength / 2, -arrowWeight / 2, arrowLength / 2, arrowWeight);

            // draw the chevron
            ctx.beginPath();
            ctx.moveTo(-arrowLength, arrowWidth);
            ctx.lineTo(0, 0);
            ctx.lineTo(-arrowLength, -arrowWidth);
            ctx.lineTo(-arrowLength * 0.8, -0);
            ctx.closePath();
            ctx.fill();
            ctx.restore();
          }
        })


      },
      initMouseHandling: function() {
        // no-nonsense drag and drop (thanks springy.js)
        selected = null;
        nearest = null;
        var dragged = null;
        var oldmass = 1

        // set up a handler object that will initially listen for mousedowns then
        // for moves and mouseups while dragging
        var handler = {
          clicked: function(e) {
            var pos = $(canvas).offset();
            _mouseP = arbor.Point(e.pageX - pos.left, e.pageY - pos.top)
            selected = nearest = dragged = particleSystem.nearest(_mouseP);

            if (dragged.node !== null) dragged.node.fixed = true

            $(canvas).bind('mousemove', handler.dragged)
            $(window).bind('mouseup', handler.dropped)

            return false
          },
          dragged: function(e) {
            var old_nearest = nearest && nearest.node._id
            var pos = $(canvas).offset();
            var s = arbor.Point(e.pageX - pos.left, e.pageY - pos.top)

            if (!nearest) return
            if (dragged !== null && dragged.node !== null) {
              var p = particleSystem.fromScreen(s)
              dragged.node.p = p
            }

            return false
          },

          dropped: function(e) {
            if (dragged === null || dragged.node === undefined) return
            if (dragged.node !== null) dragged.node.fixed = false
            dragged.node.tempMass = 50
            dragged = null
            selected = null
            $(canvas).unbind('mousemove', handler.dragged)
            $(window).unbind('mouseup', handler.dropped)
            _mouseP = null
            return false
          }
        }
        $(canvas).mousedown(handler.clicked);

      }
    }

    // helpers for figuring out where to draw arrows (thanks springy.js)
    var intersect_line_line = function(p1, p2, p3, p4) {
      var denom = ((p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y));
      if (denom === 0) return false // lines are parallel
      var ua = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denom;
      var ub = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denom;

      if (ua < 0 || ua > 1 || ub < 0 || ub > 1) return false
      return arbor.Point(p1.x + ua * (p2.x - p1.x), p1.y + ua * (p2.y - p1.y));
    }

    var intersect_line_box = function(p1, p2, boxTuple) {
      var p3 = { x: boxTuple[0], y: boxTuple[1] },
        w = boxTuple[2],
        h = boxTuple[3]

      var tl = { x: p3.x, y: p3.y };
      var tr = { x: p3.x + w, y: p3.y };
      var bl = { x: p3.x, y: p3.y + h };
      var br = { x: p3.x + w, y: p3.y + h };

      return intersect_line_line(p1, p2, tl, tr) ||
        intersect_line_line(p1, p2, tr, br) ||
        intersect_line_line(p1, p2, br, bl) ||
        intersect_line_line(p1, p2, bl, tl) ||
        false
    }

    return that
  }

})()