// CS559 Train Project
// TrainView class implementation
// see the header for details
// 

#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"

#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
#include "GL/gl.h"
#include "GL/glu.h"

#ifdef EXAMPLE_SOLUTION
#include "TrainExample/TrainExample.H"
#endif

TrainView::TrainView(int x, int y, int w, int h, const char* l) : Fl_Gl_Window(x,y,w,h,l)
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );
	resetArcball();
}

void TrainView::resetArcball()
{
	// set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this,40,250,.2f,.4f,0);
}

// FlTk Event handler for the window
// TODO: if you want to make the train respond to other events 
// (like key presses), you might want to hack this.
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event)) return 1;

	// remember what button was used
	static int last_push;

	switch(event) {
		case FL_PUSH:
			last_push = Fl::event_button();
			if (last_push == 1) {
				doPick();
				damage(1);
				return 1;
			};
			break;
		case FL_RELEASE:
			damage(1);
			last_push=0;
			return 1;
		case FL_DRAG:
			if ((last_push == 1) && (selectedCube >=0)) {
				ControlPoint* cp = &world->points[selectedCube];

				double r1x, r1y, r1z, r2x, r2y, r2z;
				getMouseLine(r1x,r1y,r1z, r2x,r2y,r2z);

				double rx, ry, rz;
				mousePoleGo(r1x,r1y,r1z, r2x,r2y,r2z, 
						  static_cast<double>(cp->pos.x), 
						  static_cast<double>(cp->pos.y),
						  static_cast<double>(cp->pos.z),
						  rx, ry, rz,
						  (Fl::event_state() & FL_CTRL) != 0);
				cp->pos.x = (float) rx;
				cp->pos.y = (float) ry;
				cp->pos.z = (float) rz;
				damage(1);
			}
			break;
			// in order to get keyboard events, we need to accept focus
		case FL_FOCUS:
			return 1;
		case FL_ENTER:	// every time the mouse enters this window, aggressively take focus
				focus(this);
				break;
		case FL_KEYBOARD:
		 		int k = Fl::event_key();
				int ks = Fl::event_state();
				if (k=='p') {
					if (selectedCube >=0) 
						printf("Selected(%d) (%g %g %g) (%g %g %g)\n",selectedCube,
							world->points[selectedCube].pos.x,world->points[selectedCube].pos.y,world->points[selectedCube].pos.z,
							world->points[selectedCube].orient.x,world->points[selectedCube].orient.y,world->points[selectedCube].orient.z);
					else
						printf("Nothing Selected\n");
					return 1;
				};
				break;
	}

	return Fl_Gl_Window::handle(event);
}

// this is the code that actually draws the window
// it puts a lot of the work into other routines to simplify things
void TrainView::draw()
{

	glViewport(0,0,w(),h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0,0,.3f,0);		// background should be blue
	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	} else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}
	// set the light parameters
	GLfloat lightPosition1[] = {0,1,1,0}; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[] = {1, 0, 0, 0};
	GLfloat lightPosition3[] = {0, -1, 0, 0};
	GLfloat yellowLight[] = {0.5f, 0.5f, .1f, 1.0};
	GLfloat whiteLight[] = {1.0f, 1.0f, 1.0f, 1.0};
	GLfloat blueLight[] = {.1f,.1f,.3f,1.0};
	GLfloat grayLight[] = {.3f, .3f, .3f, 1.0};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);



	// now draw the ground plane
	setupFloor();
	glDisable(GL_LIGHTING);
	drawFloor(200,10);
	glEnable(GL_LIGHTING);
	setupObjects();

	// we draw everything twice - once for real, and then once for
	// shadows
	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}
	
}

// note: this sets up both the Projection and the ModelView matrices
// HOWEVER: it doesn't clear the projection first (the caller handles
// that) - its important for picking
void TrainView::setProjection()
{
	// compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	if (tw->worldCam->value())
		arcball.setProjection(false);
	else if (tw->topCam->value()) {
		float wi,he;
		if (aspect >= 1) {
			wi = 110;
			he = wi/aspect;
		} else {
			he = 110;
			wi = he*aspect;
		}
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-wi,wi,-he,he,-200,200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(90,1,0,0);
	} else { //train view projection
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(60.0, 1.0, 5.0, 125.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		gluLookAt(world->trackPoints[(int)(ceilf(world->steps*world->trainU)) % world->trackPoints.size()][0],
			5 + (world->trackPoints[(int)(ceilf(world->steps*world->trainU)) % world->trackPoints.size()][1]),
			world->trackPoints[(int)(ceilf(world->steps*world->trainU)) % world->trackPoints.size()][2],
			world->trackPoints[(int)(ceilf(world->steps*world->trainU) + 1) % world->trackPoints.size()][0],
			5 + (world->trackPoints[(int)(ceilf(world->steps*world->trainU) + 1) % world->trackPoints.size()][1]),
			world->trackPoints[(int)(ceilf(world->steps*world->trainU) + 1) % world->trackPoints.size()][2],
				0.0, 1.0, 0.0);
	}
}

// this draws all of the stuff in the world
// NOTE: if you're drawing shadows, DO NOT set colors 
// (otherwise, you get colored shadows)
// this gets called twice per draw - once for the objects, once for the shadows
void TrainView::drawStuff(bool doingShadows)
{
	
	//define height/width of trackPoints
	int HEIGHT = world->steps*world->points.size();
	int WIDTH = 3;

	//resize trackPoints matrix
	world->trackPoints.resize(HEIGHT);
	for (int i = 0; i < HEIGHT; ++i) world->trackPoints[i].resize(WIDTH);

	if (tw->toggleWorldCoords->value()) //draw world reference coordinate vectors
	{
		glPushMatrix(); //BEGIN draw world reference coordinate vectors

		glTranslated(-95, 5, -95);
		glLineWidth(5.0);

		glBegin(GL_LINES);

		if (!doingShadows) //x - red
			glColor3f(1.0, 0.0, 0.0);
		glVertex3d(0, 0, 0);
		glVertex3d(10, 0, 0);

		if (!doingShadows) //y - green
			glColor3f(0.0, 1.0, 0.0);
		glVertex3d(0, 0, 0);
		glVertex3d(0, 10, 0);

		if (!doingShadows) //z - blue
			glColor3f(0.0, 0.0, 1.0);
		glVertex3d(0, 0, 0);
		glVertex3d(0, 0, 10);

		glEnd();

		glPopMatrix(); //BEGIN draw world reference coordinate vectors
	}

	// draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value()) {
		for(size_t i=0; i<world->points.size(); ++i) {
			if (!doingShadows) {
				if ( ((int) i) != selectedCube)
					glColor3ub(240,60,60);
				else
					glColor3ub(240,240,30);
			}
			world->points[i].draw();
		}
	}
	// draw the track
	drawTrack(this, doingShadows);

	// draw the train
	drawTrain(this, doingShadows);
}

void TrainView::drawTrack(TrainView* tv, bool shadows)
{

	glPushMatrix();

	if (!shadows) { glColor3ub(101,110,117); } //if we aren't drawing shadows (first call), set color

	glLineWidth(2.0); // set line width

	//Linear
	if (tw->splineBrowser->value() == 1) { linearTrack(shadows);  }

	//Cardinal Cubic 
	if (tw->splineBrowser->value() == 2) { cardinalCubicTrack(shadows); }

	//Cubic B-Spline 
	if (tw->splineBrowser->value() == 3) { cubicBSplineTrack(shadows); }

	glPopMatrix();

}

void TrainView::drawTrain(TrainView* tv, bool shadows)
{

	int controlPointNum = (int)floor(world->trainU);

	//from (A)
	fCPp->pos.x = world->trackPoints[(int)(ceilf(world->steps*world->trainU)) % world->trackPoints.size()][0];
	fCPp->pos.y = world->trackPoints[(int)(ceilf(world->steps*world->trainU)) % world->trackPoints.size()][1];
	fCPp->pos.z = world->trackPoints[(int)(ceilf(world->steps*world->trainU)) % world->trackPoints.size()][2];
	//to (B)
	tCPp->pos.x = world->trackPoints[(int)(ceilf(world->steps*world->trainU) + 1) % world->trackPoints.size()][0];
	tCPp->pos.y = world->trackPoints[(int)(ceilf(world->steps*world->trainU) + 1) % world->trackPoints.size()][1];
	tCPp->pos.z = world->trackPoints[(int)(ceilf(world->steps*world->trainU) + 1) % world->trackPoints.size()][2];
	
	float x = fCPp->pos.x;
	float y = fCPp->pos.y;
	float z = fCPp->pos.z;
	float l = 10.0;
	
	//construct basis
	Pnt3f tangentUnitVector;
	tangentUnitVector.x = tCPp->pos.x - fCPp->pos.x;
	tangentUnitVector.y = tCPp->pos.y - fCPp->pos.y;
	tangentUnitVector.z = tCPp->pos.z - fCPp->pos.z;
	Pnt3f rightUnitVector = tangentUnitVector * Pnt3f(0, 1, 0);
	Pnt3f upUnitVector = rightUnitVector * tangentUnitVector;
	
	//normalize vectors
	tangentUnitVector.normalize();
	rightUnitVector.normalize();
	upUnitVector.normalize();

	if (tw->toggleLocalCoords->value()) //draw local reference coordinate vectors
	{
		glLineWidth(5.0);
		glBegin(GL_LINES); //TEMP
		if (!shadows) //if we aren't drawing shadows (first call)
			glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3d(x, y + (0.3*l), z);
		glVertex3d(x + tangentUnitVector.x * 25, y + (0.3*l) + tangentUnitVector.y * 25, z + tangentUnitVector.z * 25);

		if (!shadows) //if we aren't drawing shadows (first call)
			glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3d(x, y + (0.3*l), z);
		glVertex3d(x + rightUnitVector.x * 25, y + (0.3*l) + rightUnitVector.y * 25, z + rightUnitVector.z * 25);

		if (!shadows) //if we aren't drawing shadows (first call)
			glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3d(x, y + (0.3*l), z);
		glVertex3d(x + upUnitVector.x * 25, y + (0.3*l) + upUnitVector.y * 25, z + upUnitVector.z * 25);

		glEnd();
	}

	float nl = -0.25f * l;
	float pl = 0.25f * l;
	
	if (!tw->trainCam->value()) // don't draw the train if you're looking out the front window
	{
		glPushMatrix();

		float M[16]; //column major ordering
		M[0] = tangentUnitVector.x; M[1] = tangentUnitVector.y; M[2] = tangentUnitVector.z;
		M[4] = upUnitVector.x; M[5] = upUnitVector.y; M[6] = upUnitVector.z;
		M[8] = rightUnitVector.x; M[9] = rightUnitVector.y; M[10] = rightUnitVector.z;
		M[12] = fCPp->pos.x; M[13] = fCPp->pos.y; M[14] = fCPp->pos.z;
		M[3] = M[7] = M[11] = 0.0f;
		M[15] = 1.0f;
		glMultMatrixf(M);
		glTranslatef(0.75*l, 2.75, 0);
		glBegin(GL_QUADS); //draw main

		//glNormal3d(0, 0, 1);
		glNormal3d(0, 0, 1);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3d(0, pl, pl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3d(-l, pl, pl);
		if (!shadows) glColor3f(0.0, 0.0, 1.0);
		glVertex3d(-l, nl, pl);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(0, nl, pl);

		//glNormal3f(rightUnitVector.x, rightUnitVector.y, rightUnitVector.z);
		glNormal3d(0, 0, -1);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3d(0, pl, nl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3d(0, nl, nl);
		if (!shadows) glColor3f(0.0, 0.0, 1.0);
		glVertex3d(-l, nl, nl);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(-l, pl, nl);

		//glNormal3f(upUnitVector.x, upUnitVector.y, upUnitVector.z);
		glNormal3d(0, 1, 0);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(0, pl, pl);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(0, pl, nl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3d(-l, pl, nl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3d(-l, pl, pl);

		glNormal3d(0, -1, 0);
		glVertex3d(0, nl, pl);
		glVertex3d(-l, nl, pl);
		glVertex3d(-l, nl, nl);
		glVertex3d(0, nl, nl);

		glNormal3d(1, 0, 0);
		glVertex3d(0, pl, pl);
		glVertex3d(0, nl, pl);
		glVertex3d(0, nl, nl);
		glVertex3d(0, pl, nl);

		glNormal3d(-1, 0, 0);
		glVertex3d(-l, pl, pl);
		glVertex3d(-l, pl, nl);
		glVertex3d(-l, nl, nl);
		glVertex3d(-l, nl, pl);

		glNormal3f(0, 0, 1);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(-l, 0.75f*l, pl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3d(-l, nl, pl);
		if (!shadows) glColor3f(0.0, 0.0, 1.0);
		glVertex3d(-l - (0.5f*l), nl, pl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3d(-l - (0.5f*l), 0.75f*l, pl);

		glNormal3d(0, 0, -1);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(-l, 0.75f*l, nl);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3d(-l, nl, nl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3d(-l - (0.5f*l), nl, nl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3d(-l - (0.5f*l), 0.75f*l, nl);

		glNormal3d(0, 1, 0);
		glVertex3d(-l, 0.75f*l, pl);
		glVertex3d(-l - (0.5f*l), 0.75f*l, pl);
		glVertex3d(-l - (0.5f*l), 0.75f*l, nl);
		glVertex3d(-l, 0.75f*l, nl);


		glNormal3d(0, -1, 0);
		glVertex3d(-l - (0.5f*l), nl, pl);
		glVertex3d(-l, nl, pl);
		glVertex3d(-l, nl, nl);
		glVertex3d(-l - (0.5f*l), nl, nl);

		glNormal3d(1, 0, 0);
		glVertex3d(-l, 0.75f*l, pl);
		glVertex3d(-l, 0.75f*l, nl);
		glVertex3d(-l, nl, nl);
		glVertex3d(-l, nl, pl);

		glNormal3d(-1, 0, 0);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3d(-l - (0.5f*l), 0.75f*l, pl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3d(-l - (0.5f*l), 0.75f*l, nl);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(-l - (0.5f*l), nl, nl);
		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3d(-l - (0.5f*l), nl, pl);

		glEnd();

		//draw smoke chute
		glBegin(GL_QUADS);

		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3f(-0.15f*l, pl, 0.5f*pl);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3f(-0.15f*l, 0.6f*l, 0.5f*pl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3f(-0.45f*l, 0.6f*l, 0.5f*pl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3f(-0.45f*l, pl, 0.5f*pl);

		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3f(-0.15f*l, pl, -0.5f*pl);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3f(-0.15f*l, 0.6f*l, -0.5f*pl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3f(-0.45f*l, 0.6f*l, -0.5f*pl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3f(-0.45f*l, pl, -0.5f*pl);


		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3f(-0.15f*l, pl, 0.5f*pl);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3f(-0.15f*l, 0.6f*l, 0.5f*pl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3f(-0.15f*l, 0.6f*l, -0.5f*pl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3f(-0.15f*l, pl, -0.5f*pl);

		if (!shadows) glColor3f(0.0, 0.0, 0.0);
		glVertex3f(-0.45f*l, pl, 0.5f*pl);
		if (!shadows) glColor3f(1.0, 0.0, 0.0);
		glVertex3f(-0.45f*l, 0.6f*l, 0.5f*pl);
		if (!shadows) glColor3f(0.0, 1.0, 0.0);
		glVertex3f(-0.45f*l, 0.6f*l, -0.5f*pl);
		if (!shadows) glColor3f(1.0, 1.0, 1.0);
		glVertex3f(-0.45f*l, pl, -0.5f*pl);

		glEnd();

		//OUTLINE TRAIN
		glLineWidth(2.0);
		if (!shadows) //if we aren't drawing shadows (first call)
			glColor3f(0.0, 0.0, 0.0);

		glBegin(GL_LINES); //draw borders

		//BEGIN front
		glVertex3d(0, pl, pl);
		glVertex3d(0, pl, nl);

		glVertex3d(0, nl, nl);
		glVertex3d(0, pl, nl);
		//END front

		//BEGIN sides
		//+z
		glVertex3d(0, pl, pl);
		glVertex3d(-l, pl, pl);

		glVertex3d(-l, pl, pl);
		glVertex3d(-l, 0.75f*l, pl);

		glVertex3d(-l, 0.75f*l, pl);
		glVertex3d(-l - (0.5f*l), 0.75f*l, pl);

		glVertex3d(-l - (0.5f*l), 0.75f*l, pl);
		glVertex3d(-l - (0.5f*l), nl, pl);

		glVertex3d(-l - (0.5f*l), nl, pl);
		glVertex3d(0, nl, pl);

		glVertex3d(0, nl, pl);
		glVertex3d(0, pl, pl);
		//-z
		glVertex3d(0, pl, nl);
		glVertex3d(-l, pl, nl);

		glVertex3d(-l, pl, nl);
		glVertex3d(-l, 0.75f*l, nl);

		glVertex3d(-l, 0.75f*l, nl);
		glVertex3d(-l - (0.5f*l), 0.75f*l, nl);

		glVertex3d(-l - (0.5f*l), 0.75f*l, nl);
		glVertex3d(-l - (0.5f*l), nl, nl);

		glVertex3d(-l - (0.5f*l), nl, nl);
		glVertex3d(0, nl, nl);

		glVertex3d(0, nl, nl);
		glVertex3d(0, pl, nl);
		//END sides

		//BEGIN top
		glVertex3d(-l - (0.5f*l), 0.75f*l, pl);
		glVertex3d(-l - (0.5f*l), 0.75f*l, nl);

		glVertex3d(-l, 0.75f*l, pl);
		glVertex3d(-l, 0.75f*l, nl);
		//END top

		//BEGIN back
		glVertex3d(-l - (0.5f*l), nl, pl);
		glVertex3d(-l - (0.5f*l), nl, nl);
		//END back
		glEnd();

		//BEGIN smoke chute
		glBegin(GL_LINES);

		glVertex3f(-0.15f*l, pl, 0.5f*pl);
		glVertex3f(-0.15f*l, 0.6f*l, 0.5f*pl);

		glVertex3f(-0.15f*l, 0.6f*l, 0.5f*pl);
		glVertex3f(-0.45f*l, 0.6f*l, 0.5f*pl);

		glVertex3f(-0.45f*l, 0.6f*l, 0.5f*pl);
		glVertex3f(-0.45f*l, pl, 0.5f*pl);

		glVertex3f(-0.15f*l, pl, -0.5f*pl);
		glVertex3f(-0.15f*l, 0.6f*l, -0.5f*pl);

		glVertex3f(-0.15f*l, 0.6f*l, -0.5f*pl);
		glVertex3f(-0.45f*l, 0.6f*l, -0.5f*pl);

		glVertex3f(-0.45f*l, 0.6f*l, -0.5f*pl);
		glVertex3f(-0.45f*l, pl, -0.5f*pl);

		glVertex3f(-0.15f*l, pl, 0.5f*pl);
		glVertex3f(-0.15f*l, 0.6f*l, 0.5f*pl);

		glVertex3f(-0.15f*l, 0.6f*l, 0.5f*pl);
		glVertex3f(-0.15f*l, 0.6f*l, -0.5f*pl);

		glVertex3f(-0.15f*l, 0.6f*l, -0.5f*pl);
		glVertex3f(-0.15f*l, pl, -0.5f*pl);

		glVertex3f(-0.45f*l, pl, 0.5f*pl);
		glVertex3f(-0.45f*l, 0.6f*l, 0.5f*pl);

		glVertex3f(-0.45f*l, 0.6f*l, 0.5f*pl);
		glVertex3f(-0.45f*l, 0.6f*l, -0.5f*pl);

		glVertex3f(-0.45f*l, 0.6f*l, -0.5f*pl);
		glVertex3f(-0.45f*l, pl, -0.5f*pl);

		glEnd();
		//END smoke chute

		glPopMatrix();
	}

	//Draw scenary
	glPushMatrix();
	
	glScalef(0.5f, 1.0f, 0.5f);
	glTranslated(2*70, 0, 2*-50);
	glRotatef(45, 0, 1, 0);
	//BEGIN tree
	glBegin(GL_QUADS);

	//trunk
	if (!shadows) glColor3ub(83, 53, 10);
	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0, 0.25f*l);

	glVertex3f(0.25f*l, 0, -0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(0.25f*l, 0, -0.25f*l);

	glVertex3f(-0.25f*l, 0, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glEnd();

	glPopMatrix();
	
	glPushMatrix();
	
	glTranslated(70, 0, -50);
	glRotatef(45, 0, 1, 0);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(1.3f, 0.75f, 1.3f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glPopMatrix();

	//tree 2
	glPushMatrix();
	glScalef(0.85f, 0.85f, 0.85f);
	glPushMatrix();

	glScalef(0.5f, 1.0f, 0.5f);
	glTranslated(2 * 82, 0, 2 * -36);
	glRotatef(-25, 0, 1, 0);
	//BEGIN tree
	glBegin(GL_QUADS);

	//trunk
	if (!shadows) glColor3ub(83, 53, 10);
	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0, 0.25f*l);

	glVertex3f(0.25f*l, 0, -0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(0.25f*l, 0, -0.25f*l);

	glVertex3f(-0.25f*l, 0, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glEnd();

	glPopMatrix();

	glPushMatrix();

	glTranslated(82, 0, -36);
	glRotatef(-25, 0, 1, 0);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(1.3f, 0.75f, 1.3f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();
	glPopMatrix();
	glPopMatrix();

	//tree3
	glPushMatrix();
	glScalef(0.9f, 0.9f, 0.9f);
	glPushMatrix();

	glScalef(0.5f, 1.0f, 0.5f);
	glTranslated(2 * -62, 0, 2 * 46);
	glRotatef(18, 0, 1, 0);
	//BEGIN tree
	glBegin(GL_QUADS);

	//trunk
	if (!shadows) glColor3ub(83, 53, 10);
	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0, 0.25f*l);

	glVertex3f(0.25f*l, 0, -0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(0.25f*l, 0, -0.25f*l);

	glVertex3f(-0.25f*l, 0, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glEnd();

	glPopMatrix();

	glPushMatrix();

	glTranslated(-62, 0, 46);
	glRotatef(18, 0, 1, 0);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(1.3f, 0.75f, 1.3f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glPopMatrix();
	glPopMatrix();

	//tree4
	glPushMatrix();
	glScalef(1.85f, 1.75f, 1.80f);
	glPushMatrix();

	glScalef(0.5f, 1.0f, 0.5f);
	glTranslated(2 * -40, 0, 2 * -20);
	glRotatef(0, 0, 1, 0);
	//BEGIN tree
	glBegin(GL_QUADS);

	//trunk
	if (!shadows) glColor3ub(83, 53, 10);
	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0, 0.25f*l);

	glVertex3f(0.25f*l, 0, -0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glVertex3f(0.25f*l, 0, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(0.25f*l, 0, -0.25f*l);

	glVertex3f(-0.25f*l, 0, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, 0.25f*l);
	glVertex3f(-0.25f*l, 0.6f*l, -0.25f*l);
	glVertex3f(-0.25f*l, 0, -0.25f*l);

	glEnd();

	glPopMatrix();

	glPushMatrix();

	glTranslated(-40, 0, -20);
	glRotatef(0, 0, 1, 0);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(1.3f, 0.75f, 1.3f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glScalef(0.9f, 0.75f, 0.9f);
	glTranslatef(0.0f, 6.0f, 0.0f);
	glBegin(GL_TRIANGLES);
	//leaves
	if (!shadows) glColor3ub(1, 121, 111);
	glVertex3f(-0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(-0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(0.5f*l, 0.6f*l, -2.0f*pl);
	glVertex3f(0.0f*l, 3.0f*l, 0.0f*pl);
	glVertex3f(0.5f*l, 0.6f*l, 2.0f*pl);

	glVertex3f(-2.0f*pl, 0.6f*l, -0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, -0.5f*l);

	glVertex3f(-2.0f*pl, 0.6f*l, 0.5f*l);
	glVertex3f(0.0f*pl, 3.0f*l, 0.0f*l);
	glVertex3f(2.0f*pl, 0.6f*l, 0.5f*l);

	glEnd();

	glPopMatrix();
	glPopMatrix();

}

void TrainView::linearTrack(bool shadows)
{

	for (size_t i = 0; i < world->points.size(); ++i)
	{
		// calculate difference vector
		float diff[] = { 
			world->points._Myfirst[(i + 1) % world->points.size()].pos.x - world->points._Myfirst[i].pos.x,
			world->points._Myfirst[(i + 1) % world->points.size()].pos.y - world->points._Myfirst[i].pos.y,
			world->points._Myfirst[(i + 1) % world->points.size()].pos.z - world->points._Myfirst[i].pos.z };
		for (int r = 0; r < world->steps; r++) //add run points
		{
			world->trackPoints[(i*world->steps) + r][0] = world->points._Myfirst[i].pos.x + (diff[0] * ((float)r / world->steps));
			world->trackPoints[(i*world->steps) + r][1] = world->points._Myfirst[i].pos.y + (diff[1] * ((float)r / world->steps));
			world->trackPoints[(i*world->steps) + r][2] = world->points._Myfirst[i].pos.z + (diff[2] * ((float)r / world->steps));
		}

	}

	for (int i = 0; i < world->points.size()*world->steps; i++)
	{
		//from (A)
		fCPp->pos.x = world->trackPoints[i][0];
		fCPp->pos.y = world->trackPoints[i][1];
		fCPp->pos.z = world->trackPoints[i][2];
		//to (B)
		tCPp->pos.x = world->trackPoints[(i + 1) % world->trackPoints.size()][0];
		tCPp->pos.y = world->trackPoints[(i + 1) % world->trackPoints.size()][1];
		tCPp->pos.z = world->trackPoints[(i + 1) % world->trackPoints.size()][2];

		//construct basis
		Pnt3f tangentUnitVector;
		tangentUnitVector.x = tCPp->pos.x - fCPp->pos.x;
		tangentUnitVector.y = tCPp->pos.y - fCPp->pos.y;
		tangentUnitVector.z = tCPp->pos.z - fCPp->pos.z;
		Pnt3f rightUnitVector = tangentUnitVector * Pnt3f(0, 1, 0);

		tangentUnitVector.normalize();
		rightUnitVector.normalize();
		
		if (!shadows) glColor3ub(101, 110, 117);
		glBegin(GL_QUADS);

		//draw track
		glVertex3f(
			fCPp->pos.x + rightUnitVector.x*3.5,
			fCPp->pos.y,
			fCPp->pos.z + rightUnitVector.z*3.5);
		glVertex3f(
			fCPp->pos.x + rightUnitVector.x*3.5,
			fCPp->pos.y-1.5,
			fCPp->pos.z + rightUnitVector.z*3.5);
		glVertex3f(
			tCPp->pos.x + rightUnitVector.x*3.5,
			tCPp->pos.y-1.5,
			tCPp->pos.z + rightUnitVector.z*3.5);
		glVertex3f(
			tCPp->pos.x + rightUnitVector.x*3.5,
			tCPp->pos.y,
			tCPp->pos.z + rightUnitVector.z*3.5);

		glVertex3f(
			fCPp->pos.x - rightUnitVector.x*3.5,
			fCPp->pos.y,
			fCPp->pos.z - rightUnitVector.z*3.5);
		glVertex3f(
			fCPp->pos.x - rightUnitVector.x*3.5,
			fCPp->pos.y-1.5,
			fCPp->pos.z - rightUnitVector.z*3.5);
		glVertex3f(
			tCPp->pos.x - rightUnitVector.x*3.5,
			tCPp->pos.y-1.5,
			tCPp->pos.z - rightUnitVector.z*3.5);
		glVertex3f(
			tCPp->pos.x - rightUnitVector.x*3.5,
			tCPp->pos.y,
			tCPp->pos.z - rightUnitVector.z*3.5);
		
		glEnd();

		//draw lines on top to be able to see track from "top" view
		glLineWidth(1.0f);
		glBegin(GL_LINES);

		if (!shadows) glColor3ub(50, 0, 0);
		glVertex3f(
			fCPp->pos.x + rightUnitVector.x*3.5,
			fCPp->pos.y,
			fCPp->pos.z + rightUnitVector.z*3.5);
		if (!shadows) glColor3ub(0, 50, 0);
		glVertex3f(
			tCPp->pos.x - rightUnitVector.x*3.5,
			tCPp->pos.y,
			tCPp->pos.z - rightUnitVector.z*3.5);

		if (!shadows) glColor3ub(0, 50, 0);
		glVertex3f(
			fCPp->pos.x + rightUnitVector.x*3.5,
			fCPp->pos.y,
			fCPp->pos.z + rightUnitVector.z*3.5);
		if (!shadows) glColor3ub(0, 0, 50);
		glVertex3f(
			tCPp->pos.x - rightUnitVector.x*3.5,
			tCPp->pos.y,
			tCPp->pos.z - rightUnitVector.z*3.5);

		glEnd();

	}
}

void TrainView::cardinalCubicTrack(bool shadows)
{

	/* 
	P1: the startpoint of the curve
	T1: the tangent (e.g. direction and speed) to how the curve leaves the startpoint
	P2: he endpoint of the curve
	T2: the tangent (e.g. direction and speed) to how the curves meets the endpoint
	*/
	/* for cardinal splines*/

	for(int t = 0; t < world->points.size(); t++)
	{

		int t_plus_one = (t + 1) % world->points.size();
		int t_plus_two = (t + 2) % world->points.size();
		int t_minus_one = (t - 1) % world->points.size();

		ControlPoint P0 = world->points._Myfirst[t_minus_one];
		ControlPoint P1 = world->points._Myfirst[t];
		ControlPoint P2 = world->points._Myfirst[t_plus_one];
		ControlPoint P3 = world->points._Myfirst[t_plus_two];

		float tension = (0.5f*-(float)tw->tension->value())+0.5;
		float T1[] = { tension *(P2.pos.x - P0.pos.x), tension *(P2.pos.y - P0.pos.y), tension *(P2.pos.z - P0.pos.z) };
		float T2[] = { tension *(P3.pos.x - P1.pos.x), tension *(P3.pos.y - P1.pos.y), tension *(P3.pos.z - P1.pos.z) };

		for (int r = 0; r < world->steps; r++)
		{

			float s = (float)r / (float)world->steps;			// scale s to go from 0 to 1
			float h1 = (2 * powf(s, 3)) - (3 * powf(s, 2)) + 1;	// calculate basis function 1
			float h2 = ((-2) * powf(s, 3)) + (3 * powf(s, 2));	// calculate basis function 2
			float h3 = powf(s, 3) - (2 * powf(s, 2)) + s;		// calculate basis function 3
			float h4 = powf(s, 3) - powf(s, 2);					// calculate basis function 4

			float h1P1[] = { h1*P1.pos.x, h1*P1.pos.y, h1*P1.pos.z };
			float h2P2[] = { h2*P2.pos.x, h2*P2.pos.y, h2*P2.pos.z };
			float h3T1[] = { h3*T1[0], h3*T1[1], h3*T1[2] };
			float h4T2[] = { h4*T2[0], h4*T2[1], h4*T2[2] };

			float p[3];
			// multiply and sum all funtions together to build the interpolated point along the curve
			for (int d = 0; d < 3; d++) { p[d] = h1P1[d] + h2P2[d] + h3T1[d] + h4T2[d]; }

			//track points
			world->trackPoints[(t*world->steps) + r][0] = p[0];
			world->trackPoints[(t*world->steps) + r][1] = p[1];
			world->trackPoints[(t*world->steps) + r][2] = p[2];

		}

	}

	for (int i = 0; i < world->steps*world->points.size(); i++)
	{
		int t = i; //because lazy
		//from (A)
		fCPp->pos.x = world->trackPoints[i][0];
		fCPp->pos.y = world->trackPoints[i][1];
		fCPp->pos.z = world->trackPoints[i][2];
		//to (B)
		tCPp->pos.x = world->trackPoints[(i + 1) % world->trackPoints.size()][0];
		tCPp->pos.y = world->trackPoints[(i + 1) % world->trackPoints.size()][1];
		tCPp->pos.z = world->trackPoints[(i + 1) % world->trackPoints.size()][2];

		//construct basis
		Pnt3f tangentUnitVector;
		tangentUnitVector.x = tCPp->pos.x - fCPp->pos.x;
		tangentUnitVector.y = tCPp->pos.y - fCPp->pos.y;
		tangentUnitVector.z = tCPp->pos.z - fCPp->pos.z;
		Pnt3f rightUnitVector = tangentUnitVector * Pnt3f(0, 1, 0);

		tangentUnitVector.normalize();
		rightUnitVector.normalize();

		if (!shadows) glColor3ub(101, 110, 117);
		glBegin(GL_QUADS);

		// draw to calculated point on the curve
		glVertex3f(world->trackPoints[t][0] + rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] + rightUnitVector.z*3.5);
		glVertex3f(world->trackPoints[t][0] + rightUnitVector.x*3.5, world->trackPoints[t][1] - 1.5, world->trackPoints[t][2] + rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			glVertex3f(world->trackPoints[0][0] + rightUnitVector.x*3.5, world->trackPoints[0][1] - 1.5, world->trackPoints[0][2] + rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[0][0] + rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] + rightUnitVector.z*3.5);
		}
		else
		{
			glVertex3f(world->trackPoints[(t + 1)][0] + rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1] - 1.5, world->trackPoints[(t + 1)][2] + rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[(t + 1)][0] + rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] + rightUnitVector.z*3.5);
		}
			
		// draw to calculated point on the curve
		glVertex3f(world->trackPoints[t][0] - rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] - rightUnitVector.z*3.5);
		glVertex3f(world->trackPoints[t][0] - rightUnitVector.x*3.5, world->trackPoints[t][1] - 1.5, world->trackPoints[t][2] - rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			glVertex3f(world->trackPoints[0][0] - rightUnitVector.x*3.5, world->trackPoints[0][1] - 1.5, world->trackPoints[0][2] - rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[0][0] - rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] - rightUnitVector.z*3.5);
		}
		else
		{
			glVertex3f(world->trackPoints[(t + 1)][0] - rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1] - 1.5, world->trackPoints[(t + 1)][2] - rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[(t + 1)][0] - rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] - rightUnitVector.z*3.5);
		}

		glEnd();

		glLineWidth(1.0f);
		glBegin(GL_LINES); //draw lines to be able to see from top view

		if (!shadows) glColor3ub(100, 0, 0);
		glVertex3f(world->trackPoints[t][0] - rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] - rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			if (!shadows) glColor3ub(0, 50, 0);
			glVertex3f(world->trackPoints[0][0] + rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] + rightUnitVector.z*3.5);
		}
		else
		{
			if (!shadows) glColor3ub(0, 50, 0);
			glVertex3f(world->trackPoints[(t + 1)][0] + rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] + rightUnitVector.z*3.5);
		}
		
		if (!shadows) glColor3ub(0, 50, 0);
		glVertex3f(world->trackPoints[t][0] + rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] + rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			if (!shadows) glColor3ub(0, 0, 50);
			glVertex3f(world->trackPoints[0][0] - rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] - rightUnitVector.z*3.5);
		}
		else
		{
			if (!shadows) glColor3ub(0, 0, 50);
			glVertex3f(world->trackPoints[(t + 1)][0] - rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] - rightUnitVector.z*3.5);
		}

		glEnd();
	}

}

void TrainView::cubicBSplineTrack(bool shadows)
{

	for (int t = 0; t < world->points.size(); t++)
	{

		int t_plus_one = (t + 1) % world->points.size();
		int t_plus_two = (t + 2) % world->points.size();
		int t_plus_three = (t + 3) % world->points.size();

		ControlPoint P0 = world->points._Myfirst[t];
		ControlPoint P1 = world->points._Myfirst[t_plus_one];
		ControlPoint P2 = world->points._Myfirst[t_plus_two];
		ControlPoint P3 = world->points._Myfirst[t_plus_three];
		
		for (int r = 0; r < world->steps; r++)
		{

			float s = (float)r / (float)world->steps;						// scale s to go from 0 to 1
			float h0 = powf(1 - s, 3) / 6;									// calculate basis function 1
			float h1 = (3 * powf(s, 3) - 6 * powf(s, 2) + 4) / 6;			// calculate basis function 2
			float h2 = (-3 * powf(s, 3) + 3 * powf(s, 2) + 3 * s + 1) / 6;	// calculate basis function 3
			float h3 = powf(s, 3) / 6;										// calculate basis function 4


			// multiply and sum all funtions together to build the interpolated point along the curve
			float p[3] =
			{
				(h0*P0.pos.x) + (h1*P1.pos.x) + (h2*P2.pos.x) + (h3*P3.pos.x),
				(h0*P0.pos.y) + (h1*P1.pos.y) + (h2*P2.pos.y) + (h3*P3.pos.y),
				(h0*P0.pos.z) + (h1*P1.pos.z) + (h2*P2.pos.z) + (h3*P3.pos.z),
			};

			//assign track points
			world->trackPoints[(t*world->steps) + r][0] = p[0];
			world->trackPoints[(t*world->steps) + r][1] = p[1];
			world->trackPoints[(t*world->steps) + r][2] = p[2];
			
		}

	}

	for (int i = 0; i < world->steps*world->points.size(); i++)
	{
		int t = i; //because lazy
		//from (A)
		fCPp->pos.x = world->trackPoints[i][0];
		fCPp->pos.y = world->trackPoints[i][1];
		fCPp->pos.z = world->trackPoints[i][2];
		//to (B)
		tCPp->pos.x = world->trackPoints[(i + 1) % world->trackPoints.size()][0];
		tCPp->pos.y = world->trackPoints[(i + 1) % world->trackPoints.size()][1];
		tCPp->pos.z = world->trackPoints[(i + 1) % world->trackPoints.size()][2];

		//construct basis
		Pnt3f tangentUnitVector;
		tangentUnitVector.x = tCPp->pos.x - fCPp->pos.x;
		tangentUnitVector.y = tCPp->pos.y - fCPp->pos.y;
		tangentUnitVector.z = tCPp->pos.z - fCPp->pos.z;
		Pnt3f rightUnitVector = tangentUnitVector * Pnt3f(0, 1, 0);

		tangentUnitVector.normalize();
		rightUnitVector.normalize();

		if (!shadows) glColor3ub(101, 110, 117);
		glBegin(GL_QUADS);

		// draw to calculated point on the curve
		glVertex3f(world->trackPoints[t][0] + rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] + rightUnitVector.z*3.5);
		glVertex3f(world->trackPoints[t][0] + rightUnitVector.x*3.5, world->trackPoints[t][1] - 1.5, world->trackPoints[t][2] + rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			glVertex3f(world->trackPoints[0][0] + rightUnitVector.x*3.5, world->trackPoints[0][1] - 1.5, world->trackPoints[0][2] + rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[0][0] + rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] + rightUnitVector.z*3.5);
		}
		else
		{
			glVertex3f(world->trackPoints[(t + 1)][0] + rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1] - 1.5, world->trackPoints[(t + 1)][2] + rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[(t + 1)][0] + rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] + rightUnitVector.z*3.5);
		}
			
		// draw to calculated point on the curve
		glVertex3f(world->trackPoints[t][0] - rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] - rightUnitVector.z*3.5);
		glVertex3f(world->trackPoints[t][0] - rightUnitVector.x*3.5, world->trackPoints[t][1] - 1.5, world->trackPoints[t][2] - rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			glVertex3f(world->trackPoints[0][0] - rightUnitVector.x*3.5, world->trackPoints[0][1] - 1.5, world->trackPoints[0][2] - rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[0][0] - rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] - rightUnitVector.z*3.5);
		}
		else
		{
			glVertex3f(world->trackPoints[(t + 1)][0] - rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1] - 1.5, world->trackPoints[(t + 1)][2] - rightUnitVector.z*3.5);
			glVertex3f(world->trackPoints[(t + 1)][0] - rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] - rightUnitVector.z*3.5);
		}
			
		glEnd();

		glLineWidth(1.0f);
		glBegin(GL_LINES);

		if (!shadows) glColor3ub(100, 0, 0);
		glVertex3f(world->trackPoints[t][0] + rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] + rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			if (!shadows) glColor3ub(0, 50, 0);
			glVertex3f(world->trackPoints[0][0] - rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] - rightUnitVector.z*3.5);
		}
		else
		{
			if (!shadows) glColor3ub(0, 50, 0);
			glVertex3f(world->trackPoints[(t + 1)][0] - rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] - rightUnitVector.z*3.5);
		}
			

		// draw to calculated point on the curve
		if (!shadows) glColor3ub(0, 50, 0);
		glVertex3f(world->trackPoints[t][0] - rightUnitVector.x*3.5, world->trackPoints[t][1], world->trackPoints[t][2] - rightUnitVector.z*3.5);
		if (t == world->steps*world->points.size() - 1)
		{
			if (!shadows) glColor3ub(0, 0, 50);
			glVertex3f(world->trackPoints[0][0] + rightUnitVector.x*3.5, world->trackPoints[0][1], world->trackPoints[0][2] + rightUnitVector.z*3.5);
		}

		else
		{
			if (!shadows) glColor3ub(0, 0, 50);
			glVertex3f(world->trackPoints[(t + 1)][0] + rightUnitVector.x*3.5, world->trackPoints[(t + 1)][1], world->trackPoints[(t + 1)][2] + rightUnitVector.z*3.5);
		}

		glEnd();

	}

}

// this tries to see which control point is under the mouse
// (for when the mouse is clicked)
// it uses OpenGL picking - which is always a trick
// TODO: if you want to pick things other than control points, or you
// changed how control points are drawn, you might need to change this
void TrainView::doPick()
{
	make_current();		// since we'll need to do some GL stuff

	int mx = Fl::event_x(); // where is the mouse?
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	// set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	gluPickMatrix((double)mx, (double)(viewport[3]-my), 5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100,buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	// draw the cubes, loading the names as we go
	for(size_t i=0; i<world->points.size(); ++i) {
		glLoadName((GLuint) (i+1));
		world->points[i].draw();
	}

	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3]-1;
	} else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n",selectedCube);
}

// CVS Header - if you don't know what this is, don't worry about it
// This code tells us where the original came from in CVS
// Its a good idea to leave it as-is so we know what version of
// things you started with
// $Header: /p/course/cs559-gleicher/private/CVS/TrainFiles/TrainView.cpp,v 1.10 2009/11/08 21:34:13 gleicher Exp $
