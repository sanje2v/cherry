/*
	Part of Cherry software
	Written by: Sanjeev Sharma
	http://sanje2v.wordpress.com/
	License: Released under public domain
*/

#include "bcm_host.h"
//#include "EGL/egl.h"
//#include "ilclient.h"
#include <iostream>
#include <algorithm>

using namespace std;

#define CHANGE_LAYER    (1<<0)
#define CHANGE_OPACITY  (1<<1)
#define CHANGE_DEST     (1<<2)
#define CHANGE_SRC      (1<<3)
#define CHANGE_MASK     (1<<4)
#define CHANGE_XFORM    (1<<5)


class VideoCoreAnimator
{
private:
	static const VC_IMAGE_TYPE_T m_imagetype = VC_IMAGE_RGB888;	// 'BGR' would have be better, unfortunately 'RGB' is all we got

	//EGLDisplay m_egldisplay;
	DISPMANX_DISPLAY_HANDLE_T m_dispmanxdisplay;
	DISPMANX_RESOURCE_HANDLE_T m_resources[2];
	DISPMANX_ELEMENT_HANDLE_T m_elements[2];
	VC_RECT_T m_rectScreen;

	void allocateImageResources(const VC_RECT_T& rect)
	{
		uint32_t pimages_on_device[2];			// Not sure what this is, no documentation

		// Create resources in GPU
		assert(m_resources[0] = vc_dispmanx_resource_create(m_imagetype,
								    rect.width,
								    rect.height,
								    &pimages_on_device[0]));
		assert(m_resources[1] = vc_dispmanx_resource_create(m_imagetype,
								    rect.width,
								    rect.height,
								    &pimages_on_device[1]));

		// Take a snapshot of desktop and use it as our first frame
		assert(vc_dispmanx_snapshot(m_dispmanxdisplay, m_resources[0], DISPMANX_NO_ROTATE) == 0);

		// Add new image elements from resources created
		DISPMANX_UPDATE_HANDLE_T update;

		assert(update = vc_dispmanx_update_start(0));

		VC_DISPMANX_ALPHA_T alpha[2] = 
		{
			{ DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0 },
			{ DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 0, 0 }
		};

		// Element 1 is currently background layer with full opacity
		m_elements[0] = vc_dispmanx_element_add(update,
							m_dispmanxdisplay,
							0,
							&rect,
							m_resources[0],
							&m_rectScreen,
							DISPMANX_PROTECTION_NONE,
							&alpha[0],
							NULL,
							DISPMANX_NO_ROTATE);
		// Element 2 is top layer with no opacity
		m_elements[1] = vc_dispmanx_element_add(update,
							m_dispmanxdisplay,
							1,
							&rect,
							m_resources[1],
							&m_rectScreen,
							DISPMANX_PROTECTION_NONE,
							&alpha[1],
							NULL,
							DISPMANX_NO_ROTATE);

		// Tell GPU to synchronize with our update;
		assert(vc_dispmanx_update_submit_sync(update) == 0);
	}

	void deallocateImageResources()
	{
		// Release all elements their associated resources
		DISPMANX_UPDATE_HANDLE_T update;

		assert(update = vc_dispmanx_update_start(0));

		vc_dispmanx_element_remove(update, m_elements[0]);
		vc_dispmanx_element_remove(update, m_elements[1]);
		vc_dispmanx_update_submit_sync(update);
		vc_dispmanx_resource_delete(m_resources[0]);
		vc_dispmanx_resource_delete(m_resources[1]);
	}

public:
	VideoCoreAnimator()
	{
		// Initialize member variables
		m_dispmanxdisplay = -1;
		m_resources[0] = m_resources[1] = 0;
		m_elements[0] = m_elements[1] = 0;
		m_rectScreen.x = m_rectScreen.y = m_rectScreen.width = m_rectScreen.height = 0;

		// Initialize Broadcom VideoCore IV host
		bcm_host_init();

		//assert((m_egldisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY)) != EGL_NO_DISPLAY);
		//assert(eglInitialize(m_egldisplay, NULL, NULL) != EGL_FALSE);
		assert((m_dispmanxdisplay = vc_dispmanx_display_open(DISPMANX_ID_MAIN_LCD)) >= 0);

		// NOTE: The retrieved screen size is multiplied by 2^16
		// This value is used in element creation. Due to lack of
		// documentation, it is not clear why this needs to be done.
		getDisplayRect(m_rectScreen);
		vc_dispmanx_rect_set(&m_rectScreen, 0, 0, m_rectScreen.width << 16, m_rectScreen.height << 16);

		VC_RECT_T rectScreen;
		getDisplayRect(rectScreen);
		allocateImageResources(rectScreen);
	}

	~VideoCoreAnimator()
	{
		deallocateImageResources();

		//eglTerminate(m_egldisplay);
		vc_dispmanx_display_close(m_dispmanxdisplay);
	}

	void getDisplaySize(int32_t& width, int32_t& height)
	{
		DISPMANX_MODEINFO_T info;

		assert(vc_dispmanx_display_get_info(m_dispmanxdisplay, &info) >= 0);
		width = info.width;
		height = info.height;
	}

	void getDisplayRect(VC_RECT_T& rect)
	{
		int32_t width, height;

		getDisplaySize(width, height);
		vc_dispmanx_rect_set(&rect, 0, 0, width, height);
	}

	void getDisplayInfo(DISPMANX_TRANSFORM_T& transform, DISPLAY_INPUT_FORMAT_T& input_format)
	{
		DISPMANX_MODEINFO_T info;

		assert(vc_dispmanx_display_get_info(m_dispmanxdisplay, &info) >= 0);
		transform = info.transform;
		input_format = info.input_format;
	}

	void fillDisplay(uint8_t red, uint8_t green, uint8_t blue)
	{
		DISPMANX_UPDATE_HANDLE_T update;

		update = vc_dispmanx_update_start(0);
		vc_dispmanx_display_set_background(update, m_dispmanxdisplay, red, green, blue);
		vc_dispmanx_update_submit_sync(update);
	}

	void animateTransition(void *pimagedata, const int pitch, const VC_RECT_T& rect)
	{
		// Write images data to GPU resource
		assert(vc_dispmanx_resource_write_data(m_resources[1],
							m_imagetype,
							pitch,
							pimagedata,
							&rect) == 0);

		// Notify GPU we have modified an element data, so be ready to update
		DISPMANX_UPDATE_HANDLE_T update;

		assert(update = vc_dispmanx_update_start(0));
		vc_dispmanx_element_modified(update, m_elements[1], &rect);
		vc_dispmanx_update_submit_sync(update);

		for (int16_t i = 0; i <= 255; i += 5)	// NOTE: Beware, using 'int8_t' for 'i' will result in unwanted overflow
		{
			assert(update = vc_dispmanx_update_start(0));

			// Change opacity for transition effect
			vc_dispmanx_element_change_attributes(update,
								m_elements[1],
								CHANGE_OPACITY,
								0,
								(int8_t) i,
								NULL,
								NULL,
								0,
								DISPMANX_NO_ROTATE);

			// Tell GPU to synchronize with our update;
			assert(vc_dispmanx_update_submit_sync(update) == 0);

			usleep(500);
		}

		// Change opacity of bottom element and set its layer order as top
		// For the previous top layer, make it bottom
		assert(update = vc_dispmanx_update_start(0));

		vc_dispmanx_element_change_attributes(update,
							m_elements[0],
							CHANGE_LAYER | CHANGE_OPACITY,
							1,
							0,
							NULL,
							NULL,
							0,
							DISPMANX_NO_ROTATE);
		vc_dispmanx_element_change_attributes(update,
							m_elements[1],
							CHANGE_LAYER,
							0,
							0,
							NULL,
							NULL,
							0,
							DISPMANX_NO_ROTATE);
		// Tell GPU to synchronize with our update;
		assert(vc_dispmanx_update_submit_sync(update) == 0);

		// Swap resource and element handles so that in next update
		// the new top layer is modified
		swap(m_resources[0], m_resources[1]);
		swap(m_elements[0], m_elements[1]);
	}
};
