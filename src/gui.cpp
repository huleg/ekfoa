#include "gui.hpp"

boost::mutex Gui::lock_;

Arcball Gui::arcball_;
GLfloat Gui::zoom_ = 0.5f;
GLboolean Gui::is_rotating_ = GL_FALSE;

Delaunay Gui::triangulation_;
Point3d Gui::closest_point_;

GLFWwindow* Gui::window_;

std::list<Eigen::Vector3d> Gui::trajectory_;
Eigen::Matrix3d Gui::axes_orientation_and_confidence_;
std::vector<Point3d> Gui::XYZs_[3];

//========================================================================
// Initialize Miscellaneous OpenGL state
//========================================================================

void Gui::init(void){
	int width, height;

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		exit(EXIT_FAILURE);

	window_ = glfwCreateWindow(640, 600, "EKF Obstacle avoidance", NULL, NULL);
	if (!window_) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwSetWindowPos(window_, 1040, 430);


	glfwSetKeyCallback(window_, key_callback);
	glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
	glfwSetMouseButtonCallback(window_, mouse_button_callback);
	glfwSetCursorPosCallback(window_, cursor_position_callback);
	glfwSetScrollCallback(window_, scroll_callback);

	glfwMakeContextCurrent(window_);
	glfwSwapInterval(1);

	glfwGetFramebufferSize(window_, &width, &height);
	framebuffer_size_callback(window_, width, height);
    // Use Gouraud (smooth) shading
    glShadeModel(GL_SMOOTH);

    // Switch on the z-buffer
    glEnable(GL_DEPTH_TEST);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    // Background color is white
    glClearColor(1, 1, 1, 0);

    //Large spherical points
    glEnable( GL_POINT_SMOOTH );

    //Limit the number of renderings to 60 per second
    glfwSwapInterval(1);
}



//========================================================================
// Print errors
//========================================================================

void Gui::error_callback(int error, const char* description){
    fprintf(stderr, "Error: %s\n", description);
}


//========================================================================
// Handle key strokes
//========================================================================

void Gui::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
//    if (action != GLFW_PRESS)
//        return;
//
//    switch (key) {
//        case GLFW_KEY_ESCAPE:
//            glfwSetWindowShouldClose(window, GL_TRUE);
//            break;
//        default:
//            break;
//    }
}


//========================================================================
// Callback function for mouse button events
//========================================================================

void Gui::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    if (action == GLFW_PRESS){
    	double x, y;
    	glfwGetCursorPos(window_, &x, &y);
    	arcball_.startRotation(x, y);
    	is_rotating_ = GL_TRUE;
    } else {
    	is_rotating_ = GL_FALSE;
    	arcball_.stopRotation();
    }
}


//========================================================================
// Callback function for cursor motion events
//========================================================================

void Gui::cursor_position_callback(GLFWwindow* window, double x, double y){
    if (is_rotating_)
    	arcball_.updateRotation(x, y);
}


//========================================================================
// Callback function for scroll events
//========================================================================

void Gui::scroll_callback(GLFWwindow* window, double x, double y){
    zoom_ -= (float) y / 4.f;
    if (zoom_ < 0)
        zoom_ = 0;
}


//========================================================================
// Callback function for framebuffer resize events
//========================================================================

void Gui::framebuffer_size_callback(GLFWwindow* window, int width, int height){
	std::cout << "framebuffer_size_callback()" << std::endl;
    double window_ratio = 1;

    if (height > 0)
        window_ratio = width / height;

    // Setup viewport
    glViewport(0, 0, width, height);

    // Setup rotator:
    arcball_.setWidthHeight(width, height);

    // Change to the projection matrix and set our viewing volume
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, window_ratio, 0.1f, 1024.0);
}


void Gui::update_draw_parameters(const std::list<Eigen::Vector3d> & trajectory, const Eigen::Matrix3d & axes_orientation_and_confidence, std::vector<Point3d> (& XYZs)[3], Delaunay & triangulation, Point3d & closest_point){
	//lock
	lock_.lock();

	trajectory_ = trajectory;

	axes_orientation_and_confidence_ = axes_orientation_and_confidence;

	XYZs_[0] = XYZs[0];
	XYZs_[1] = XYZs[1];
	XYZs_[2] = XYZs[2];

	triangulation_ = triangulation;

	closest_point_ = closest_point;

    //unlock
    lock_.unlock();
}

//========================================================================
// redraw
//========================================================================

bool Gui::redraw(){
	if (glfwWindowShouldClose(window_))
	    	return false;

    // Clear the color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // We don't want to modify the projection matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


    //Lock
    lock_.lock();

    // Move back
    glTranslatef(0, 0, -zoom_);
    // Rotate the view
    glRotatef(180, 1, 0, 0);//TODO: integrate this rotation to the arcball.


    arcball_.applyRotationMatrix();

    //Draw "drone":
    draw_drone();

    //Draw surface:
    draw_surface();
    lock_.unlock();

    glfwSwapBuffers(window_);
    glfwPollEvents();
//    glfwWaitEvents();

    return true;
}

void Gui::draw_drone(){
    //Draw trajectory:
    glPointSize(0.5);
    glColor4f(0.7, 0.7, 0.7, 0.2);
    glBegin(GL_POINTS);
    for (std::list<Eigen::Vector3d>::iterator pos = trajectory_.begin(); pos != trajectory_.end(); pos++){
    	glVertex3f((*pos)(0), (*pos)(1), (*pos)(2));
    }
    glEnd();

    //Draw the camera position/orientation, where the length of each axis is proportional to its confidence (comes from the covariance matrix)
    glLineWidth(3.0);
    glBegin(GL_LINES);
    //each line goes from current position to the point that represents the confidence and orientation according to the current orientation quaternion and covariance matrix. X = red, Y = green and Z = blue
    glColor3f(1, 0, 0);
    glVertex3f(trajectory_.back()(0), trajectory_.back()(1), trajectory_.back()(2));
    glVertex3f(axes_orientation_and_confidence_.col(0)(0), axes_orientation_and_confidence_.col(0)(1), axes_orientation_and_confidence_.col(0)(2));
    glColor3f(0, 1, 0);
    glVertex3f(trajectory_.back()(0), trajectory_.back()(1), trajectory_.back()(2));
    glVertex3f(axes_orientation_and_confidence_.col(1)(0), axes_orientation_and_confidence_.col(1)(1), axes_orientation_and_confidence_.col(1)(2));
    glColor3f(0, 0, 1);
    glVertex3f(trajectory_.back()(0), trajectory_.back()(1), trajectory_.back()(2));
    glVertex3f(axes_orientation_and_confidence_.col(2)(0), axes_orientation_and_confidence_.col(2)(1), axes_orientation_and_confidence_.col(2)(2));
    glEnd();
}


void Gui::draw_surface(){
	glPointSize(10.0);

	glColor3f(0, 1.f, 1.f);
	glBegin(GL_POINTS);
	glVertex3f(closest_point_.x(), closest_point_.y(), closest_point_.z());

	glEnd();

	//Points
	glPointSize(4.0);

	glColor3f(1, 0, 1);
	glBegin(GL_POINTS);
	//Draw each point mean estimated position:
	for (size_t i=0 ; i<XYZs_[0].size() ; i++){
		glVertex3f(XYZs_[0][i].x(), XYZs_[0][i].y(), XYZs_[0][i].z());
	}
	glEnd();

	//Inverse depth uncertainty:
	glLineWidth(1.0);
	glColor3f(0, 0, 0);
	glBegin(GL_LINES);
	//Draw each point depth uncertainty as a line between the -3*sigma and 3*sigma of the mean:
	for (size_t i=0 ; i<XYZs_[1].size() ; i++){
		glVertex3f(XYZs_[1][i].x(), XYZs_[1][i].y(), XYZs_[1][i].z()); // [1]==close
		glVertex3f(XYZs_[2][i].x(), XYZs_[2][i].y(), XYZs_[2][i].z()); // [2]==far
	}
	glEnd();

	//surface:
	glColor3f(0.6, 0.4, 0.7);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	std::list<Triangle> surface_faces;
	glBegin(GL_TRIANGLES);
	for(Delaunay::Finite_faces_iterator fit = triangulation_.finite_faces_begin(); fit != triangulation_.finite_faces_end(); ++fit) {
		const Delaunay::Face_handle & face = fit;
		//face->vertex(i)->info() = index of the point in the observation list.
		//The surface is built with a pesimistic approach...the closest point of the 99.73% of the distribution mass
		glVertex3f(XYZs_[1][face->vertex(0)->info()].x(), XYZs_[1][face->vertex(0)->info()].y(), XYZs_[1][face->vertex(0)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(1)->info()].x(), XYZs_[1][face->vertex(1)->info()].y(), XYZs_[1][face->vertex(1)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(2)->info()].x(), XYZs_[1][face->vertex(2)->info()].y(), XYZs_[1][face->vertex(2)->info()].z());

	}
	glEnd();

	glLineWidth(1.0);
	glColor3f(0, 0, 0);
	glBegin(GL_LINES);
	//Draw the segment lines betweeen each two points in the surface:
	for(Delaunay::Finite_faces_iterator fit = triangulation_.finite_faces_begin(); fit != triangulation_.finite_faces_end(); ++fit) {
		const Delaunay::Face_handle & face = fit;
		//face->vertex(i)->info() = index of the point in the observation list.
		glVertex3f(XYZs_[1][face->vertex(0)->info()].x(), XYZs_[1][face->vertex(0)->info()].y(), XYZs_[1][face->vertex(0)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(1)->info()].x(), XYZs_[1][face->vertex(1)->info()].y(), XYZs_[1][face->vertex(1)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(1)->info()].x(), XYZs_[1][face->vertex(1)->info()].y(), XYZs_[1][face->vertex(1)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(2)->info()].x(), XYZs_[1][face->vertex(2)->info()].y(), XYZs_[1][face->vertex(2)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(2)->info()].x(), XYZs_[1][face->vertex(2)->info()].y(), XYZs_[1][face->vertex(2)->info()].z());
		glVertex3f(XYZs_[1][face->vertex(0)->info()].x(), XYZs_[1][face->vertex(0)->info()].y(), XYZs_[1][face->vertex(0)->info()].z());
	}

	glEnd();
}

void Gui::release(){
	glfwDestroyWindow(window_);
	glfwTerminate();
}
