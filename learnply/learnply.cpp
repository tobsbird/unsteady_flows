#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <iostream>

#include "glError.h"
#include "gl/glew.h"
#include "gl/freeglut.h"
#include "ply.h"
#include "icVector.H"
#include "icMatrix.H"
#include "polyhedron.h"
#include "polyline.h"
#include "trackball.h"
#include "tmatrix.h"

//using namespace std;
Polyhedron* poly;

//streamline variables
bool showPickedPoint, showStreamLines, showSingularities;
icVector3 pickedPoint;
PolyLine streamline_picked;
std::vector<PolyLine> streamlines;

/*scene related variables*/
const float zoomspeed = 0.9;
const int view_mode = 0;		// 0 = othogonal, 1=perspective
const double radius_factor = 1.0;
int win_width = 1000;
int win_height = 1000;
float aspectRatio = win_width / win_height;
/*
Use keys 1 to 0 to switch among different display modes.
Each display mode can be designed to show one type 
visualization result.

Predefined ones: 
display mode 1: solid rendering
display mode 2: show wireframes
display mode 3: render each quad with colors of vertices
*/
int display_mode = 1;

int showCoordinate = 1;
int setInterval = 1;
int ShowDots = 1;

/*User Interaction related variabes*/
float s_old, t_old;
float rotmat[4][4];
double zoom = 0.8;
double translation[2] = { 0, 0 };
int mouse_mode = -2;	// -1 = no action, 1 = tranlate y, 2 = rotate

/*IBFV related variables*/
//https://www.win.tue.nl/~vanwijk/ibfv/
#define	NPN 64
#define SCALE 4.0
int    Npat = 32;
int    iframe = 0;
float  tmax = win_width / (SCALE*NPN);
float  dmax = SCALE / win_width;
unsigned char *pixels;

#define DM  ((float) (1.0/(100-1.0)))

/******************************************************************************
Forward declaration of functions
******************************************************************************/

void init(void);
void makePatterns(void);

/*glut attaching functions*/
void keyboard(unsigned char key, int x, int y);
void motion(int x, int y);
void display(void);
void mouse(int button, int state, int x, int y);
void mousewheel(int wheel, int direction, int x, int y);
void reshape(int width, int height);

/*functions for element picking*/
void display_vertices(GLenum mode, Polyhedron* poly);
void display_quads(GLenum mode, Polyhedron* poly);
void display_selected_vertex(Polyhedron* poly);
void display_selected_quad(Polyhedron* poly);

/*display vis results*/
void display_polyhedron(Polyhedron* poly);



typedef void function2d (double*, double*, double*, double*);
void setVector(function2d equation, double *x, double *y, double*vx, double*vy)

{
	equation(x,y,vx,vy);
}

void processMenuEvents(int);

/*display utilities*/

/*
draw a sphere
x, y, z are the coordiate of the dot
radius of the sphere 
R: the red channel of the color, ranges [0, 1]
G: the green channel of the color, ranges [0, 1]
B: the blue channel of the color, ranges [0, 1]
*/
void drawDot(double x, double y, double z, double radius = 0.1, double R = 1.0, double G = 0.0, double B = 0.0) {

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);

	GLfloat mat_diffuse[4];

	{
		mat_diffuse[0] = R;
		mat_diffuse[1] = G;
		mat_diffuse[2] = B;
		mat_diffuse[3] = 1.0;
	}

	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);

	GLUquadric* quad = gluNewQuadric();

	glPushMatrix();
	glTranslatef(x, y, z);
	gluSphere(quad, radius, 50, 50);
	glPopMatrix();

	gluDeleteQuadric(quad);
}

/*
draw a line segment
width: the width of the line, should bigger than 0
R: the red channel of the color, ranges [0, 1]
G: the green channel of the color, ranges [0, 1]
B: the blue channel of the color, ranges [0, 1]
*/
void drawLineSegment(LineSegment ls, double width = 1.0, double R = 1.0, double G = 0.0, double B = 0.0) {

	glDisable(GL_LIGHTING);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(width);

	glBegin(GL_LINES);
	glColor3f(R, G, B);
	glVertex3f(ls.start.x, ls.start.y, ls.start.z);
	glVertex3f(ls.end.x, ls.end.y, ls.end.z);
	glEnd();

	glDisable(GL_BLEND);
}

/*
draw a polyline
width: the width of the line, should bigger than 0
R: the red channel of the color, ranges [0, 1]
G: the green channel of the color, ranges [0, 1]
B: the blue channel of the color, ranges [0, 1]
*/
void drawPolyline(PolyLine pl, double width = 1.0, double R = 1.0, double G = 0.0, double B = 0.0) {
	
	glDisable(GL_LIGHTING);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(width);

	glBegin(GL_LINES);
	glColor3f(R, G, B);

	for (int i = 0; i < pl.size(); i++) {
		glVertex3f(pl[i].start.x, pl[i].start.y, pl[i].start.z);
		glVertex3f(pl[i].end.x, pl[i].end.y, pl[i].end.z);
	}

	glEnd();

	glDisable(GL_BLEND);
}




/******************************************************************************
Main program.
******************************************************************************/
int main(int argc, char* argv[])
{
	/*load mesh from ply file*/
	//FILE* this_file = fopen("../quadmesh_2D/vector_data/saddle.ply", "r");
	FILE* this_file = fopen("../quadmesh_2D/new_vector_data/v10.ply", "r");

	poly = new Polyhedron(this_file);
	fclose(this_file);
	
	/*initialize the mesh*/
	poly->initialize(); // initialize the mesh
	poly->write_info();
	poly->setMinMax();

	/*init glut and create window*/
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowPosition(20, 20);
	glutInitWindowSize(win_width, win_height);
	glutCreateWindow("Scientific Visualization");
	int menu = glutCreateMenu(processMenuEvents);
	glutSetMenuFont(menu, GLUT_BITMAP_TIMES_ROMAN_24);
	glutAddMenuEntry("Show/Hide Coordinate Sys", 0);
	glutAddMenuEntry("Show/Hide Dots", 1);
	glutAddMenuEntry("Unsteady Field", 2);
	glutAddMenuEntry("Steady Field", 3);
	glutAddMenuEntry("Ambient Motion", 4);
	glutAddMenuEntry("Reset", 5);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
	/*initialize openGL*/
	init();

	/*prepare the noise texture for IBFV*/
	makePatterns();
	
	/*the render function and callback registration*/
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutMotionFunc(motion);
	glutMouseFunc(mouse);
	glutMouseWheelFunc(mousewheel);
	
	/*event processing loop*/
	glutMainLoop();
	
	/*clear memory before exit*/
	poly->finalize();	// finalize everything
	free(pixels);
	return 0;
}





/******************************************************************************
Set projection mode
******************************************************************************/

void set_view(GLenum mode)
{
	GLfloat light_ambient0[] = { 0.3, 0.3, 0.3, 1.0 };
	GLfloat light_diffuse0[] = { 0.7, 0.7, 0.7, 1.0 };
	GLfloat light_specular0[] = { 0.0, 0.0, 0.0, 1.0 };

	GLfloat light_ambient1[] = { 0.0, 0.0, 0.0, 1.0 };
	GLfloat light_diffuse1[] = { 0.5, 0.5, 0.5, 1.0 };
	GLfloat light_specular1[] = { 0.0, 0.0, 0.0, 1.0 };

	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular0);

	glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1);


	glMatrixMode(GL_PROJECTION);
	if (mode == GL_RENDER)
		glLoadIdentity();

	if (aspectRatio >= 1.0) {
		if (view_mode == 0)
			glOrtho(-radius_factor * zoom * aspectRatio, radius_factor * zoom * aspectRatio, -radius_factor * zoom, radius_factor * zoom, -1000, 1000);
		else
			glFrustum(-radius_factor * zoom * aspectRatio, radius_factor * zoom * aspectRatio, -radius_factor* zoom, radius_factor* zoom, 0.1, 1000);
	}
	else {
		if (view_mode == 0)
			glOrtho(-radius_factor * zoom, radius_factor * zoom, -radius_factor * zoom / aspectRatio, radius_factor * zoom / aspectRatio, -1000, 1000);
		else
			glFrustum(-radius_factor * zoom, radius_factor * zoom, -radius_factor* zoom / aspectRatio, radius_factor* zoom / aspectRatio, 0.1, 1000);
	}


	GLfloat light_position[3];
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	light_position[0] = 5.5;
	light_position[1] = 0.0;
	light_position[2] = 0.0;
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	light_position[0] = -0.1;
	light_position[1] = 0.0;
	light_position[2] = 0.0;
	glLightfv(GL_LIGHT2, GL_POSITION, light_position);
}

/******************************************************************************
Update the scene
******************************************************************************/

void set_scene(GLenum mode, Polyhedron* poly)
{
	glTranslatef(translation[0], translation[1], -3.0);

	/*multiply rotmat to current mat*/
	{
		int i, j, index = 0;

		GLfloat mat[16];

		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				mat[index++] = rotmat[i][j];

		glMultMatrixf(mat);
	}

	glScalef(0.9 / poly->radius, 0.9 / poly->radius, 0.9 / poly->radius);
	glTranslatef(-poly->center.entry[0], -poly->center.entry[1], -poly->center.entry[2]);
}


/******************************************************************************
Init scene
******************************************************************************/

void init(void) {

	mat_ident(rotmat);

	/* select clearing color */
	glClearColor(0.0, 0.0, 0.0, 0.0);  // background
	glShadeModel(GL_FLAT);
	glPolygonMode(GL_FRONT, GL_FILL);

	glDisable(GL_DITHER);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	//set pixel storage modes
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	
	glEnable(GL_NORMALIZE);
	if (poly->orientation == 0)
		glFrontFace(GL_CW);
	else
		glFrontFace(GL_CCW);
}


/******************************************************************************
Pick objects from the scene
******************************************************************************/

int processHits(GLint hits, GLuint buffer[])
{
	unsigned int i, j;
	GLuint names, * ptr;
	double smallest_depth = 1.0e+20, current_depth;
	int seed_id = -1;
	unsigned char need_to_update;

	ptr = (GLuint*)buffer;
	for (i = 0; i < hits; i++) {  /* for each hit  */
		need_to_update = 0;
		names = *ptr;
		ptr++;

		current_depth = (double)*ptr / 0x7fffffff;
		if (current_depth < smallest_depth) {
			smallest_depth = current_depth;
			need_to_update = 1;
		}
		ptr++;
		current_depth = (double)*ptr / 0x7fffffff;
		if (current_depth < smallest_depth) {
			smallest_depth = current_depth;
			need_to_update = 1;
		}
		ptr++;
		for (j = 0; j < names; j++) {  /* for each name */
			if (need_to_update == 1)
				seed_id = *ptr - 1;
			ptr++;
		}
	}
	return seed_id;
}

/******************************************************************************
Diaplay all quads for selection
******************************************************************************/

void display_quads(GLenum mode, Polyhedron* this_poly)
{
	unsigned int i, j;
	GLfloat mat_diffuse[4];

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	//glDisable(GL_LIGHTING);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (i = 0; i < this_poly->nquads; i++) {
		if (mode == GL_SELECT)
			glLoadName(i + 1);

		Quad* temp_q = this_poly->qlist[i];
		{
			mat_diffuse[0] = 1.0;
			mat_diffuse[1] = 1.0;
			mat_diffuse[2] = 0.0;
			mat_diffuse[3] = 1.0;
		}
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		
		glBegin(GL_POLYGON);
		for (j = 0; j < 4; j++) {
			Vertex* temp_v = temp_q->verts[j];
			//glColor3f(0, 0, 0);
			glVertex3d(temp_v->x, temp_v->y, temp_v->z);
		}
		glEnd();
	}
}

/******************************************************************************
Diaplay all vertices for selection
******************************************************************************/

void display_vertices(GLenum mode, Polyhedron* this_poly)
{
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (int i = 0; i < this_poly->nverts; i++) {
		if (mode == GL_SELECT)
			glLoadName(i + 1);

		Vertex* temp_v = this_poly->vlist[i];

		{
			GLUquadric* quad = gluNewQuadric();

			glPushMatrix();
			glTranslatef(temp_v->x, temp_v->y, temp_v->z);
			glColor4f(0, 0, 1, 1.0);
			gluSphere(quad, this_poly->radius * 0.01, 50, 50);
			glPopMatrix();

			gluDeleteQuadric(quad);
		}
	}
}

/******************************************************************************
Diaplay selected quad
******************************************************************************/

void display_selected_quad(Polyhedron* this_poly)
{
	if (this_poly->selected_quad == -1)
	{
		return;
	}

	unsigned int i, j;
	GLfloat mat_diffuse[4];

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);

	Quad* temp_q = this_poly->qlist[this_poly->selected_quad];

	glBegin(GL_POLYGON);
	for (j = 0; j < 4; j++) {
		Vertex* temp_v = temp_q->verts[j];
		glColor3f(1.0, 0.0, 1.0);
		glVertex3d(temp_v->x, temp_v->y, 0.0);
	}
	glEnd();
}

/******************************************************************************
Diaplay selected vertex
******************************************************************************/

void display_selected_vertex(Polyhedron* this_poly)
{
	if (this_poly->selected_vertex == -1)
	{
		return;
	}

	Vertex* temp_v = this_poly->vlist[this_poly->selected_vertex];

	drawDot(temp_v->x, temp_v->y, temp_v->z, this_poly->radius * 0.01, 1.0, 0.0, 0.0);

}


/******************************************************************************
Callback function for glut window reshaped
******************************************************************************/

void reshape(int width, int height) {

	win_width = width;
	win_height = height;

	aspectRatio = (float)width / (float)height;

	glViewport(0, 0, width, height);

	set_view(GL_RENDER);

	/*Update pixels buffer*/
	free(pixels);
	pixels = (unsigned char *)malloc(sizeof(unsigned char)*win_width*win_height * 3);
	memset(pixels, 255, sizeof(unsigned char)*win_width*win_height * 3);
}


/******************************************************************************
Callback function for dragging mouse
******************************************************************************/

void motion(int x, int y) {
	float r[4];
	float s, t;

	s = (2.0 * x - win_width) / win_width;
	t = (2.0 * (win_height - y) - win_height) / win_height;

	if ((s == s_old) && (t == t_old))
		return;

	switch (mouse_mode) {
	case 2:

		Quaternion rvec;

		mat_to_quat(rotmat, rvec);
		trackball(r, s_old, t_old, s, t);
		add_quats(r, rvec, rvec);
		quat_to_mat(rvec, rotmat);

		s_old = s;
		t_old = t;

		display();
		break;

	case 1:

		translation[0] += (s - s_old);
		translation[1] += (t - t_old);

		s_old = s;
		t_old = t;

		display();
		break;
	}
}

/******************************************************************************
Callback function for mouse clicks
******************************************************************************/

void mouse(int button, int state, int x, int y) {

	int key = glutGetModifiers();

	if (button == GLUT_LEFT_BUTTON || button == GLUT_RIGHT_BUTTON) {
		
		if (state == GLUT_DOWN) {
			float xsize = (float)win_width;
			float ysize = (float)win_height;

			float s = (2.0 * x - win_width) / win_width;
			float t = (2.0 * (win_height - y) - win_height) / win_height;

			s_old = s;
			t_old = t;

			/*translate*/
			if (button == GLUT_LEFT_BUTTON)
			{
				mouse_mode = 1;
			}

			/*rotate*/
			if (button == GLUT_RIGHT_BUTTON)
			{
				mouse_mode = 2;
			}
		}
		else if (state == GLUT_UP) {

			if (button == GLUT_LEFT_BUTTON && key == GLUT_ACTIVE_SHIFT) {  // build up the selection feedback mode

				/*select face*/

				GLuint selectBuf[512];
				GLint hits;
				GLint viewport[4];

				glGetIntegerv(GL_VIEWPORT, viewport);

				glSelectBuffer(win_width, selectBuf);
				(void)glRenderMode(GL_SELECT);

				glInitNames();
				glPushName(0);

				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();

				/*create 5x5 pixel picking region near cursor location */
				gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y), 1.0, 1.0, viewport);

				set_view(GL_SELECT);
				set_scene(GL_SELECT, poly);
				display_quads(GL_SELECT, poly);

				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glFlush();

				glMatrixMode(GL_MODELVIEW);

				hits = glRenderMode(GL_RENDER);
				poly->selected_quad = processHits(hits, selectBuf);
				printf("Selected quad id = %d\n", poly->selected_quad);
				glutPostRedisplay();

			}
			else if (button == GLUT_LEFT_BUTTON && key == GLUT_ACTIVE_CTRL)
			{
				/*select vertex*/

				GLuint selectBuf[512];
				GLint hits;
				GLint viewport[4];

				glGetIntegerv(GL_VIEWPORT, viewport);

				glSelectBuffer(win_width, selectBuf);
				(void)glRenderMode(GL_SELECT);

				glInitNames();
				glPushName(0);

				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();

				/*  create 5x5 pixel picking region near cursor location */
				gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y), 1.0, 1.0, viewport);

				set_view(GL_SELECT);
				set_scene(GL_SELECT, poly);
				display_vertices(GL_SELECT, poly);

				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glFlush();

				glMatrixMode(GL_MODELVIEW);

				hits = glRenderMode(GL_RENDER);
				poly->selected_vertex = processHits(hits, selectBuf);
				printf("Selected vert id = %d\n", poly->selected_vertex);
				glutPostRedisplay();

			}

			mouse_mode = -1;
		}
	}
}

/******************************************************************************
Callback function for mouse wheel scroll
******************************************************************************/

void mousewheel(int wheel, int direction, int x, int y) {
	if (direction == 1) {
		zoom *= zoomspeed;
		glutPostRedisplay();
	}
	else if (direction == -1) {
		zoom /= zoomspeed;
		glutPostRedisplay();
	}
}



void testEqu1(double* x, double* y, double* vx, double* vy) {
	*vx = sin(iframe);
	*vy = 1;
}
void testEqu2(double* x, double* y, double* vx, double* vy) {
	*vx = cos(*y+iframe);
	*vy = sin(*x-0.5*iframe);
}

void testEqu3(double* x, double* y, double* vx, double* vy) {
	*vx = 1.5 * (*x);
	*vy = *y*cos(*x-0.5 *iframe);
}

/******************************************************************************
Callback function for scene display
******************************************************************************/
std::vector<std::pair<PolyLine, float>> contours;

void display(void)
{
	
	glClearColor(1.0, 1.0, 1.0, 1.0);  // background for rendering color coding and lighting

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	set_view(GL_RENDER);
	CHECK_GL_ERROR();

	set_scene(GL_RENDER, poly);
	CHECK_GL_ERROR();

	/*display the mesh*/
	display_polyhedron(poly);
	CHECK_GL_ERROR();

	/*display selected elements*/
	display_selected_vertex(poly);
	CHECK_GL_ERROR();

	display_selected_quad(poly);
	CHECK_GL_ERROR();

	glFlush();
	glutSwapBuffers();
	glFinish();

	CHECK_GL_ERROR();
}

void keyboard(unsigned char key, int x, int y) {
	int i;

	/* set escape key to exit */
	switch (key) {
		case 27: {
			poly->finalize();  // finalize_everything
			exit(0);
			break;
		}
		case '1': {
			display_mode = 1;
			break;
		}
		case '2': {
			//steady field
			display_mode = 2;
			break;
		}
		case '3': {
			//Ambient Motion
			display_mode = 3;
		}

		case 'c':{
			showCoordinate = !showCoordinate;
			break;
		}
		case 's': {
			setInterval = !setInterval;
			break;
		}
		case 'd': {
			ShowDots = !ShowDots;
			break;
		}
		case 'r':{
			mat_ident(rotmat);
			translation[0] = 0;
			translation[1] = 0;
			zoom = 1.0;
			glutPostRedisplay();
			break;
		}
	}
}

//glutAddMenuEntry("Show/Hide Coordinate Sys", 0);
//glutAddMenuEntry("Show/Hide Dots", 1);
//glutAddMenuEntry("Unsteady Field", 2);
//glutAddMenuEntry("Steady Field", 3);
//glutAddMenuEntry("Ambient Motion", 4);
//glutAddMenuEntry("Reset", 5);

void processMenuEvents(int option) {

	switch (option) {
		case 0: {
			showCoordinate = !showCoordinate;
			break;
		}
		case 1: {
			ShowDots = !ShowDots;
			break;
		}
		case 2: {
			display_mode = 1;
			break;
		}
		case 3: {
			display_mode = 2;
			break;
		}
		case 4: {
			display_mode = 3;
			break;
		}
		case 5: {
			mat_ident(rotmat);
			translation[0] = 0;
			translation[1] = 0;
			zoom = 1.0;
			glutPostRedisplay();
			break;
		}
	}
}
/******************************************************************************
Diaplay the polygon with visualization results
******************************************************************************/

void drawUnsteady(Polyhedron* poly, double width = 1.0, double R = 1.0, double G = 0.0, double B = 0.0) {

	glDisable(GL_LIGHTING);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(width);

	glBegin(GL_LINES);
	glColor3f(R, G, B);
	for (int i = 0; i < poly->nverts; i++) {
		Vertex* temp_v = poly->vlist[i];
		
		////////////////////////////////////////////////////
		//testCase1
		//vx = sin(t);
		//vy = 1;
		//Remove the next line
		setVector(testEqu1, &temp_v->x, &temp_v->y, &temp_v->vx, &temp_v->vy);

		////////////////////////////////////////////////////
		//testCase2
		//vx = cos(y + t)
		//vy = sin(x - 0.5 * t)
		//Remove the next line
		//setVector(testEqu2, &temp_v->x, &temp_v->y, &temp_v->vx, &temp_v->vy);


		//printf("%f  %f\n", temp_v->vx, temp_v->vy); 
		glVertex3f(temp_v->x, temp_v->y, 0);
		double x = temp_v->x; 
		double y = temp_v->y;
		double vx = temp_v->vx;
		double vy = temp_v->vy;
		double k = (vy) / (vx);
		double angle = atan(k);
		//double k = 1 / pow(x, 2.0);
		/*vx = 1 / sqrt(1.0 + pow(k, 2.0));
		if (k > 0) {
			vx = 1 / sqrt(1.0 + pow(k, 2.0));
			vy = k * vx;
		}
		else {
			vx = -1 / sqrt(1.0 + pow(k, 2.0));
			vy = k * vx;
		}*/
		glVertex3f(vx + x, vy + y, 0);

		//glVertex3f(temp_v->vx * tan(angle) + temp_v->x, temp_v->vy * tan(angle) + temp_v->y, 0);
	}
	iframe += 1;
	glEnd();

	glDisable(GL_BLEND);
}

void drawDotsInField(Polyhedron* poly, double radius = 0.1, double R = 1.0, double G = 0.0, double B = 0.0) {

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);

	GLfloat mat_diffuse[4];

	{
		mat_diffuse[0] = R;
		mat_diffuse[1] = G;
		mat_diffuse[2] = B;
		mat_diffuse[3] = 1.0;
	}

	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);

	GLUquadric* quad = gluNewQuadric();
	glPointSize(5.0f);
	for (int i = 0; i < poly->nverts; i++) {
		Vertex* temp_v = poly->vlist[i];
		glBegin(GL_POINTS);
		glColor3f(1.0f, 0.0f, 0.0f);
		
		glVertex3f(temp_v->x, temp_v->y, 0);
		glEnd();
	}

	gluDeleteQuadric(quad);
}

void drawCordinate(Polyhedron* poly){ 
	if (showCoordinate) {
		drawLineSegment(LineSegment(icVector3(poly->minx, 0, 0), icVector3(poly->maxx, 0, 0)), 3);
		drawLineSegment(LineSegment(icVector3(0, poly->miny, 0), icVector3(0, poly->maxy, 0)), 3);
	}
}

void drawDots(Polyhedron* poly) {
	if (ShowDots) {
		drawDotsInField(poly);
	}
}


void convertToSteadyVec2d(int i, int j, Vertex& v, Vertex& J, Vertex& f, Vertex& g) {
	double x = v.x, y = v.y, vx = v.vx, vy = v.vy;

}

void display_polyhedron(Polyhedron* poly)
{
	if (setInterval)
		Sleep(200);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);

	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	CHECK_GL_ERROR();
	switch (display_mode) {
		case 1:{
			//displayIBFV();
			drawUnsteady(poly);
			break;
			
		}
		case 2: {
			drawUnsteady(poly);
			break;
		}
		case 3: {
			drawUnsteady(poly);
			break;
		}
			  
	}
	drawCordinate(poly);
	drawDots(poly);
	
	if (iframe == INT_MAX)
		iframe = 0;
}
