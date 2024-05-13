# -Freeform-curves
 Freeform curves editor in C++ using OpenGL
In this task, I implemented a Lagrange, Bézier, and Catmull-Rom spline editor in world coordinate space. The entire parameter range for each curve is [0,1]. Lagrange and Catmull-Rom curves divide this range with knot values such that the difference between consecutive knot values is proportional to the distance between control points. The point size is 10 and the line thickness is 2. The control points are displayed in maximum intensity red, while the curve is displayed in maximum intensity yellow. The viewport completely covers the 600x600 resolution user window. In the virtual world, the unit of distance is [m], i.e., meters.

Initially, the camera window is centered at the origin of the world coordinate system with a size of 30x30 [m]. The user can adjust the camera window using keyboard inputs, allowing panning and zooming:

’Z’: Increases the size of the camera window by 1.1 times while maintaining the center (zoom-out).
’z’: Decreases the size of the camera window by 1/1.1 times while maintaining the center (zoom-in).
’P’: Moves the camera window to the right by 1 meter (pan).
’p’: Moves the camera window to the left by 1 meter (pan).

Upon pressing these keys, the image of the current curve immediately changes according to the new camera window.

The type of curve to be defined next can be determined by the following key presses:
’l’: Lagrange
’b’: Bézier
’c’: Catmull-Rom

Upon pressing these keys, if the current curve exists, it is destroyed, and you can begin specifying the control points for the new curve. When the left mouse button is pressed, the cursor is placed under the control point, i.e., the input pipeline must produce the inverse of the output transformation. By pressing the right mouse button, a nearby (within 10 centimeters) existing control point can be selected. The selected control point follows the cursor until the right button is released. Pressing the 'T' key increases the tension parameter of the current and future Catmull-Rom curves by 0.1, while pressing the 't' key decreases it by the same amount. Changing the control point and tension parameter immediately affects the shape of the curve; thus, it needs to be redrawn.
