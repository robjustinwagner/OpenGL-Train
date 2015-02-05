CS 559 PROJECT 1
BOB WAGNER
ID: 906-424-8603
CS LOGIN: bob


+ FEATURE LIST
==============
- toggle buttons/options to draw local (train) and world reference coordinate systems
- enhanced train model & coloration
- train has proper 3d orientation (but can not go up-side down on loops)
- Train View implemented properly
- track enhancements: 
	> dual rails
	> track depth (QUADS)
	> lines on top of the QUADS such that the track can be seen from top view (QUADS have 0 width)
	> cool track coloration (vertex color interpolation)
- linear interpolation
- cardinal cubic (C(1)) w/ variable tension parameter (1 being "tight" (basically linear), to -1 being very "loose")
- Cubic B-Spline (C(2))
- I believe Arc Length Parameterization is nearly implemented (see code), but not working 100%, 
	some issues exist with velocity and resets train at control point
- some interesting scenary
- minor UI modifications to increase asthetics


+ DISCUSSION OF TECHNICAL DETAILS
=================================
- to compute local coordinate system for train, I find tangent by subtracting trackpoint[i+1] by trackpoint[i] (e.g. trackpoint[i+1] - trackpoint[i]). this gives us a tiny vector pointing towards where we need to go. This works because the error is so small it is not noticable/ negligble. The right/normal vector is computed by taking the cross product between the tangent vector and the world up vector (0,1,0). The local "up" vector is computed by taking the croos product between the normal/right vector and the tangent vector.

- in computing the arc-length, I basically did the same procedure done in the tutorial, approximating the curve with small segments and finding the overall length. I then incremented by s and found the corresponding u value, so that the advancement of the train was regulated by the arc-length, and not by parameter.

- linear curve is subtraction by control points, draw line from one control point to the next, increment by number of steps for each control point

- cardinal cubic and cubic b-spline are essentially the same, but with different basis functions. The first interpolates the control points (must pass through them), the latter approximates the control points (and therefore does not pass through them). the points are calculated by incrementing by number of steps, done for each of the control points.