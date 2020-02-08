#include <X11/Xlib.h>
#include <stdio.h>
#include <fftw3.h>
#include <float.h>

#define N 100

struct array{
	fftw_complex *data;
	double max[2], min[2];
};

void set_maxmin(struct array *a){
	a->max[0] = a->max[1] = -DBL_MAX;
	a->min[0] = a->min[1] = DBL_MAX;
	for(int i = 0; i < N; ++i){
		for(int j = 0; j < 2; ++j){
			if(a->max[j] < a->data[i][j]) a->max[j] = a->data[i][j];
			if(a->min[j] > a->data[i][j]) a->min[j] = a->data[i][j];
		}
	}
}

void set_points(XPoint *points, struct array *a, int part, int x, int y, int interval, int height){
	for(int i = 0; i < N; ++i){
		points[i].x = x + interval * i;
		points[i].y = y + 10 + (height - 20) * (a->max[part] - a->data[i][part]) / (a->max[part] - a->min[part]);
	}
}

int main(){
	Display *display = XOpenDisplay(NULL);
	if(display == NULL){
		fputs("error: XOpenDisplay failed\n", stderr);
		return -1;
	}

	int defaultscreen = XDefaultScreen(display);
	int max_width = XDisplayWidth(display, defaultscreen);
	int max_height = XDisplayHeight(display, defaultscreen);
	Window rootWindow = XRootWindow(display, defaultscreen);
	int defaultDepth = XDefaultDepth(display, defaultscreen);
	unsigned long black = XBlackPixel(display, defaultscreen);
	unsigned long white = XWhitePixel(display, defaultscreen);
	GC defaultGC = XDefaultGC(display, defaultscreen);

	int width = max_width, height = max_height;
	Window window = XCreateSimpleWindow(display, rootWindow, 0, 0, width, height, 0, black, black);

	Pixmap buffer = XCreatePixmap(display, window, width, height, defaultDepth);
	XGCValues xgc;
	xgc.foreground = black;
	GC blackGC = XCreateGC(display, buffer, GCForeground, &xgc);
	xgc.foreground = white;
	GC whiteGC = XCreateGC(display, buffer, GCForeground, &xgc);

	struct array x = {
		.data = fftw_malloc(sizeof(fftw_complex) * N)
	}, y = {
		.data = fftw_malloc(sizeof(fftw_complex) * N)
	};
	fftw_plan forward = fftw_plan_dft_1d(N, x.data, y.data, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_plan backward = fftw_plan_dft_1d(N, y.data, x.data, FFTW_BACKWARD, FFTW_ESTIMATE);

	for(int i = 0; i < N; ++i){
		x.data[i][0] = i;
		x.data[i][1] = -i;
	}

	fftw_execute(forward);

	int interval = (width - 1) / (N - 1);
	XPoint points[N * 4];
	XSegment segments[3];

	int height_quarter = height - 4;
	set_maxmin(&x);
	set_maxmin(&y);
	set_points(points, &x, 0, 0, 0, interval, height_quarter);
	set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
	set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
	set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);

	XSelectInput(display, window, ExposureMask | ButtonPressMask | Button1MotionMask | ButtonReleaseMask | StructureNotifyMask);
	XMapWindow(display, window);

	XEvent event;

	for(;;){
		XNextEvent(display, &event);
		switch(event.type){
			case Expose:
				XCopyArea(display, buffer, window, defaultGC, 0, 0, width, height, 0, 0);
				break;
			case DestroyNotify:
				XFreeGC(display, blackGC);
				XFreeGC(display, whiteGC);
				XFreePixmap(display, buffer);
				XCloseDisplay(display);
				fftw_destroy_plan(forward);
				fftw_destroy_plan(backward);
				fftw_free(x.data);
				fftw_free(y.data);
				return 0;
			case ButtonPress:
				if(event.xbutton.y < height_quarter){
					x.data[(event.xbutton.x + interval / 2) / interval][0]
						= x.max[0] - (event.xbutton.y - 10) * (x.max[0] - x.min[0]) / (height_quarter - 20);
					fftw_execute(forward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points, &x, 0, 0, 0, interval, height_quarter);
					set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
					set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				}else if(event.xbutton.y < height_quarter * 2){
					x.data[(event.xbutton.x + interval / 2) / interval][1]
						= x.max[1] - (event.xbutton.y - height_quarter - 10) * (x.max[1] - x.min[1]) / (height_quarter - 20);
					fftw_execute(forward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
					set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
					set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				}else if(event.xbutton.y < height_quarter * 3){
					y.data[(event.xbutton.x + interval / 2) / interval][0]
						= y.max[0] - (event.xbutton.y - height_quarter * 2 - 10) * (y.max[0] - y.min[0]) / (height_quarter - 20);
					fftw_execute(backward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points, &x, 0, 0, 0, interval, height_quarter);
					set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
					set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
				}else if(event.xbutton.y < height_quarter * 3){
					y.data[(event.xbutton.x + interval / 2) / interval][1]
						= y.max[1] - (event.xbutton.y - height_quarter * 3 - 10) * (y.max[1] - y.min[1]) / (height_quarter - 20);
					fftw_execute(backward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points, &x, 0, 0, 0, interval, height_quarter);
					set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
					set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				}
				XFillRectangle(display, buffer, blackGC, 0, 0, width, height);
				XDrawPoints(display, buffer, whiteGC, points, sizeof points / sizeof points[0], CoordModeOrigin);
				XDrawSegments(display, buffer, whiteGC, segments, sizeof segments / sizeof segments[0]);
				XClearArea(display, window, 0, 0, width, height, True);
				break;
			case MotionNotify:
				if(event.xbutton.y < height_quarter){
					x.data[(event.xbutton.x + interval / 2) / interval][0]
						= x.max[0] - (event.xbutton.y - 10) * (x.max[0] - x.min[0]) / (height_quarter - 20);
					fftw_execute(forward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points, &x, 0, 0, 0, interval, height_quarter);
					set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
					set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				}else if(event.xbutton.y < height_quarter * 2){
					x.data[(event.xbutton.x + interval / 2) / interval][1]
						= x.max[1] - (event.xbutton.y - height_quarter - 10) * (x.max[1] - x.min[1]) / (height_quarter - 20);
					fftw_execute(forward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
					set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
					set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				}else if(event.xbutton.y < height_quarter * 3){
					y.data[(event.xbutton.x + interval / 2) / interval][0]
						= y.max[0] - (event.xbutton.y - height_quarter * 2 - 10) * (y.max[0] - y.min[0]) / (height_quarter - 20);
					fftw_execute(backward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points, &x, 0, 0, 0, interval, height_quarter);
					set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
					set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
				}else if(event.xbutton.y < height_quarter * 3){
					y.data[(event.xbutton.x + interval / 2) / interval][1]
						= y.max[1] - (event.xbutton.y - height_quarter * 3 - 10) * (y.max[1] - y.min[1]) / (height_quarter - 20);
					fftw_execute(backward);
					set_maxmin(&x);
					set_maxmin(&y);
					set_points(points, &x, 0, 0, 0, interval, height_quarter);
					set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
					set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				}
				XFillRectangle(display, buffer, blackGC, 0, 0, width, height);
				XDrawPoints(display, buffer, whiteGC, points, sizeof points / sizeof points[0], CoordModeOrigin);
				XDrawSegments(display, buffer, whiteGC, segments, sizeof segments / sizeof segments[0]);
				XClearArea(display, window, 0, 0, width, height, True);
				break;
			case ConfigureNotify:
				width = event.xconfigure.width;
				height = event.xconfigure.height;
				height_quarter = height / 4;
				set_points(points, &x, 0, 0, 0, interval, height_quarter);
				set_points(points + N, &x, 1, 0, height_quarter, interval, height_quarter);
				set_points(points + N * 2, &y, 0, 0, height_quarter * 2, interval, height_quarter);
				set_points(points + N * 3, &y, 1, 0, height_quarter * 3, interval, height_quarter);
				segments[0].x1 = segments[1].x1 = segments[2].x1 = 0;
				segments[0].x2 = segments[1].x2 = segments[2].x2 = width;
				segments[0].y1 = segments[0].y2 = height_quarter;
				segments[1].y1 = segments[1].y2 = height_quarter * 2;
				segments[2].y1 = segments[2].y2 = height_quarter * 3;
				XFillRectangle(display, buffer, blackGC, 0, 0, width, height);
				XDrawPoints(display, buffer, whiteGC, points, sizeof points / sizeof points[0], CoordModeOrigin);
				XDrawSegments(display, buffer, whiteGC, segments, sizeof segments / sizeof segments[0]);
				XClearArea(display, window, 0, 0, width, height, True);
				break;
		}
	}
}
