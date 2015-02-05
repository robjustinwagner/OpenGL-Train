/* CS559 - FALL '14 - Programming Assignment 1 - Trains and Roller Coasters*
 Framework written by Michael Gleicher, October 2008
 Remaining content written by Bob Wagner, October 2014
*/

// file: Train.cpp - the "main file" and entry point for the train project
// Generates window and inserts world widget

//includes
#include "stdio.h"
#include "TrainWindow.H"

#pragma warning(push)
#pragma warning(disable:4312)
#pragma warning(disable:4311)
#include <Fl/Fl.h>
#pragma warning(pop)

int main(int, char**)
{
	printf("READING RAINBOW: Train & Roller Coaster, CS559 Project 1 by Bob Wagner");

	TrainWindow tw;
	tw.show();

	Fl::run();
}

// $Header: /p/course/cs559-gleicher/private/CVS/TrainFiles/Train.cpp,v 1.2 2008/10/14 02:52:12 gleicher Exp $