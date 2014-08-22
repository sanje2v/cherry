/*
	Part of Cherry software
	Written by: Sanjeev Sharma
	http://sanje2v.wordpress.com/
	License: Released under public domain
*/

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <opencv.hpp>
#include "cherry_banner.h"
#include "vcanimator.h"		// VideoCoreAnimator class

// Fancy bolding and red colouring in terminal
#define SET_BOLD_RED_STYLE(x)	"\033[1;31m" x "\033[0m"


using namespace std;
using namespace cv;

static bool gExit = false;	// Flag used to terminate 'while' loop in 'main()'

void signal_Terminate(int);	// Handles both 'SIGTERM' and 'SIGINT' signals
void enumerateImageFiles(const string& path, vector<string>& files);
void correctImage(Mat& imgData, const VC_RECT_T& rect);


int main(int argc, char *argv[])
{
	static const char szImagePath[] = "/home/pi/.cherry/images/";

	// Show banner
	system("clear");	// Clear terminal window
	cout << SET_BOLD_RED_STYLE(CHERRY_BANNER) << endl
		<< "This is a GPU accelerated slideshow software." << endl
		<< "License: Released under public domain" << endl
		<< "Press Ctrl-C to terminate. Now loading..." << endl << endl;

	// Hook same signal handler for 'Ctrl-C' interrupt and process terminate signal
	signal(SIGINT, signal_Terminate);
	signal(SIGTERM, signal_Terminate);

	// Enumerate picture files under 'szImagePath'
	vector<string> files;
	enumerateImageFiles(szImagePath, files);

	if (files.empty())	// No image found or directory path does not exists, so exit
		cout << "No image files found in '" << szImagePath << "', exiting!" << endl;
	else
	{
		sleep(3);	// Wait for the message to be displayed and read

		VideoCoreAnimator vcanim;

		// Make note of screen rectangle
		VC_RECT_T rectScreen;

		vcanim.getDisplayRect(rectScreen);

		if (files.size() == 1)	// If there is only one image, display it and sleep
		{
			Mat imgData = imread(files[0].c_str());
			correctImage(imgData, rectScreen);

			vcanim.animateTransition(imgData.data, imgData.step, rectScreen);

			while (!gExit)
				sleep(3600);	// Sleep the whole day, nothing else todo
		}
		else			// For multiple images, display them in circular fashion
		{
			size_t i = 0;

			while (!gExit)	// Do this until, program is asked to terminate
			{
				Mat imgData = imread(files[i].c_str());	// Read next image in the list
				correctImage(imgData, rectScreen);	// Make corrections in image format and size

				// Push this frame to make animation transition from previous frame to this
				vcanim.animateTransition(imgData.data, imgData.step, rectScreen);
				i = (i + 1) % files.size();		// Circular index to use vector as circular buffer

				sleep(2);				// Wait until next transition
			}
		}
	}

	return 0;
}

void signal_Terminate(int)
{
	// Notifies 'while' loop in 'main()' to terminate
	gExit = true;
}

void enumerateImageFiles(const string& path, vector<string>& files)
{
	// Enumerate image files that are in supported formats
	assert(!path.empty());
	assert(path.back() == '/');

	// Open directory for enumeration
	DIR *dir = opendir(path.c_str());
	if (!dir) return;

	// Go through each file and directory inside 'path'
	dirent *direntry;
	while ((direntry = readdir(dir)) != NULL)
	{
		string targetfile(path + direntry->d_name);	// Create full path with filename
		struct stat st;

		stat(targetfile.c_str(), &st);			// Query file information
		if (S_ISREG(st.st_mode))			// Is this a file?
		{
			// Check if it is of specific image type that is supported
			static const size_t EXTENSION_LENGTH = 4;
			string fileext(targetfile.substr(targetfile.size() - EXTENSION_LENGTH));

			if (fileext == ".bmp" || fileext == ".png" ||
				fileext == "jpeg" || fileext == ".jpg" ||
				fileext == "tiff" || fileext == ".tif")
				files.push_back(targetfile);	// If everything's ok, save image path
		}
	}

	closedir(dir);
}

void correctImage(Mat& imgData, const VC_RECT_T& rect)
{
	// Check if 'imgData' is of size 'rect'
	// If not, resize it
	if (imgData.cols != rect.width || imgData.rows != rect.height)
		resize(imgData, imgData, Size(rect.width, rect.height), 0, 0, INTER_LINEAR);

	// OpenCV loads image in 'BGR888' format but GPU wants it in 'RGB888'
	// Hence, this conversion
	cvtColor(imgData, imgData, CV_BGR2RGB);
}
