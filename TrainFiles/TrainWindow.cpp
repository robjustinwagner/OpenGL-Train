// CS559 Train Project -
// Train Window class implementation
// - note: this is code that a student might want to modify for their project
//   see the header file for more details
// - look for the TODO: in this file
// - also, look at the "TrainView" - its the actual OpenGL window that
//   we draw into
//
// Written by Mike Gleicher, October 2008
//

#include "TrainWindow.H"
#include "TrainView.H"
#include "CallBacks.H"

#include <FL/fl.h>
#include <FL/Fl_Box.h>

// for using the real time clock
#include <time.h>



/////////////////////////////////////////////////////
TrainWindow::TrainWindow(const int x, const int y) : Fl_Double_Window(x,y,800,600,"Train & Roller Coaster, CS559 Project 1 by Bob Wagner")
{
	// make all of the widgets
	begin();	// add to this widget
	{
		int pty=5;			// where the last widgets were drawn

		trainView = new TrainView(5,5,590,590);
		trainView->tw = this;
		trainView->world = &world;
		this->resizable(trainView);

		// to make resizing work better, put all the widgets in a group
		widgets = new Fl_Group(600,5,190,590);
		widgets->begin();

		runButton = new Fl_Button(605,pty,60,20,"Run");
		togglify(runButton);

		Fl_Button* fb = new Fl_Button(700,pty,25,20,"@>>");
		fb->callback((Fl_Callback*)forwCB,this);
		Fl_Button* rb = new Fl_Button(670,pty,25,20,"@<<");
		rb->callback((Fl_Callback*)backCB,this);
		
		arcLength = new Fl_Button(730,pty,65,20,"ArcLength");
		togglify(arcLength,0);
  
		pty+=25;
		speed = new Fl_Value_Slider(655,pty,140,20,"speed");
		speed->range(0,5);
		speed->value(2);
		speed->align(FL_ALIGN_LEFT);
		speed->type(FL_HORIZONTAL);

		pty += 50;

		// camera buttons - in a radio button group
		Fl_Group* camGroup = new Fl_Group(600,pty,195,20);
		camGroup->begin();
		worldCam = new Fl_Button(605, pty, 60, 20, "World");
        worldCam->type(FL_RADIO_BUTTON);		// radio button
        worldCam->value(1);			// turned on
        worldCam->selection_color((Fl_Color)3); // yellow when pressed
		worldCam->callback((Fl_Callback*)damageCB,this);
		trainCam = new Fl_Button(670, pty, 60, 20, "Train");
        trainCam->type(FL_RADIO_BUTTON);
        trainCam->value(0);
        trainCam->selection_color((Fl_Color)3);
		trainCam->callback((Fl_Callback*)damageCB,this);
		topCam = new Fl_Button(735, pty, 60, 20, "Top");
        topCam->type(FL_RADIO_BUTTON);
        topCam->value(0);
        topCam->selection_color((Fl_Color)3);
		topCam->callback((Fl_Callback*)damageCB,this);
		camGroup->end();

		pty += 25;

		// coordinate toggle buttons - in a radio button group
		Fl_Group* coordinateToggleGroup = new Fl_Group(600, pty, 195, 20);
		coordinateToggleGroup->begin();
		toggleWorldCoords = new Fl_Button(610, pty, 87.5, 20, "World Ref.");
		toggleWorldCoords->type(FL_RADIO_BUTTON);		// radio button
		togglify(toggleWorldCoords, 1);					// toggle-able and turned on initially
		toggleWorldCoords->selection_color((Fl_Color)3); // yellow when pressed
		toggleWorldCoords->callback((Fl_Callback*)damageCB, this);
		toggleLocalCoords = new Fl_Button(702.5, pty, 87.5, 20, "Local Ref.");
		toggleLocalCoords->type(FL_RADIO_BUTTON);
		togglify(toggleLocalCoords, 0);					// toggle-able and turned on initially
		toggleLocalCoords->selection_color((Fl_Color)3);
		toggleLocalCoords->callback((Fl_Callback*)damageCB, this);
		coordinateToggleGroup->end();

		pty += 50;

		// browser to select spline types
		splineBrowser = new Fl_Browser(605,pty,190,60,"Spline Type");
		splineBrowser->type(2);		// select
		splineBrowser->callback((Fl_Callback*)damageCB,this);
		splineBrowser->add("Linear");
		splineBrowser->add("Cardinal Cubic");
		splineBrowser->add("Cubic B-Spline");
		splineBrowser->select(1);

		pty += 80;

		tension = new Fl_Value_Slider(655, pty, 140, 20, "tension");
		tension->callback((Fl_Callback*)damageCB, this);
		tension->range(-1, 1);
		tension->value(0.15);
		tension->align(FL_ALIGN_LEFT);
		tension->type(FL_HORIZONTAL);

		pty += 50;

		// add and delete points
		Fl_Button* ap = new Fl_Button(610, pty, 87.5, 20, "Add Point");
		ap->callback((Fl_Callback*)addPointCB,this);
		Fl_Button* dp = new Fl_Button(702.5, pty, 87.5, 20, "Delete Point");
		dp->callback((Fl_Callback*)deletePointCB,this);

		pty += 25;
		// reset the points
		resetButton = new Fl_Button(735,pty,60,20,"Reset");
		resetButton->callback((Fl_Callback*)resetCB,this);
		Fl_Button* loadb = new Fl_Button(605,pty,60,20,"Load");
		loadb->callback((Fl_Callback*) loadCB, this);
		Fl_Button* saveb = new Fl_Button(670,pty,60,20,"Save");
		saveb->callback((Fl_Callback*) saveCB, this);

		pty += 25;



		// roll the points
		/*
		Fl_Button* rx = new Fl_Button(605,pty,30,20,"R+X");
		rx->callback((Fl_Callback*)rpxCB,this);
		Fl_Button* rxp = new Fl_Button(635,pty,30,20,"R-X");
		rxp->callback((Fl_Callback*)rmxCB,this);
		Fl_Button* rz = new Fl_Button(670,pty,30,20,"R+Z");
		rz->callback((Fl_Callback*)rpzCB,this);
		Fl_Button* rzp = new Fl_Button(700,pty,30,20,"R-Z");
		rzp->callback((Fl_Callback*)rmzCB,this);
		*/

		// we need to make a little phantom widget to have things resize correctly
		Fl_Box* resizebox = new Fl_Box(600,595,200,5);
		widgets->resizable(resizebox);

		widgets->end();
	}
	end();	// done adding to this widget

	// set up callback on idle
	Fl::add_idle((void (*)(void*))runButtonCB,this);
}

// handy utility to make a button into a toggle
void TrainWindow::togglify(Fl_Button* b, int val)
{
    b->type(FL_TOGGLE_BUTTON);		// toggle
    b->value(val);		// turned off
    b->selection_color((Fl_Color)3); // yellow when pressed	
	b->callback((Fl_Callback*)damageCB,this);
}

void TrainWindow::damageMe()
{
	if (trainView->selectedCube >= ((int)world.points.size()))
		trainView->selectedCube = 0;
	trainView->damage(1);
}

/////////////////////////////////////////////////////
// this will get called (approximately) 30 times per second
// if the run button is pressed
void TrainWindow::advanceTrain(float dir)
{
	static bool first = true;

	if (arcLength->value()) {
		float vel = dir * (float)speed->value();
		world.trainU = arclenVtoV(first, world.trainU, vel, this);
		first = false;
	}
	else
	{
		first = true;
		world.trainU += dir * ((float)speed->value() * (1 / (float)world.steps));
	}

	float nct = static_cast<float>(world.points.size());
	if (world.trainU > nct) world.trainU -= nct;
	if (world.trainU < 0) world.trainU += nct;

}

float TrainWindow::arclenVtoV(bool first, float trainU, float vel, TrainWindow* tw)
{
	static vector<float> arcParamPoints;
	static vector<float> uParamPoints;
	static int s = 0;
	if (first)
	{
		int numPoints = 25;

		arcParamPoints.resize(numPoints + 1);
		uParamPoints.resize(numPoints + 1);
		float last = 0;
		//make u|s table (as in tutorial)
		for (int u = 0; u <= numPoints; u++)
		{
			uParamPoints[u] = ((float)u / (float)numPoints);
			if (u == 0) arcParamPoints[u] = 0;
			else arcParamPoints[u] = sqrt(powf(tw->world.trackPoints[(int)floor(uParamPoints[u] * world.steps)][0] - tw->world.trackPoints[(int)floor(uParamPoints[u - 1] * world.steps)][0], 2) + powf(tw->world.trackPoints[(int)floor(uParamPoints[u] * world.steps)][1] - tw->world.trackPoints[(int)floor(uParamPoints[u - 1] * world.steps)][0], 2) + powf(tw->world.trackPoints[(int)floor(uParamPoints[u] * world.steps)][2] - tw->world.trackPoints[(int)floor(uParamPoints[u - 1] * world.steps)][0], 2)) + last;
			last = arcParamPoints[u];

		}
	}

	if (s == 0)
	{
		s++;
		return 0;
	}
	else //find proper u value using s
	{
		bool found = false;
		int i;
		int max_val = floor(arcParamPoints._Mylast[0]);
		for (i = 1; i < arcParamPoints.size() && !found; i++)
		{

			if (arcParamPoints[i] > trainU)
			{
				found = true;
			}

		}
		float ratio = (s - arcParamPoints[i - 1]) / (arcParamPoints[i] - arcParamPoints[i - 1]);
		s = (s + (1 * vel));
		s = s % (max_val + 1);
		return (uParamPoints[i] + (uParamPoints[i]*ratio));
		//return vel * (1 / (float)tw->world.steps);  //temporary
	}
}