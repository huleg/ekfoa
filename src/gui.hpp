#ifndef GUI_H_
#define GUI_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <iostream>
#include <list>

#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include <opencv2/core/core.hpp> //Point2f

#include <vector>   //vector
#include <Eigen/Core>  //Eigen::Vector3d
#include <Eigen/Geometry> //Eigen::Quaternion

#include "feature.hpp"
#include "kalman.hpp"
#include "motion_model.hpp"
#include "opengl_utils/arcball.hpp"

//Delaunay triangulation:
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
typedef CGAL::Exact_predicates_exact_constructions_kernel         K;
typedef CGAL::Triangulation_vertex_base_with_info_2<size_t, K>    Vb;
typedef CGAL::Triangulation_data_structure_2<Vb>                  Tds;
typedef CGAL::Delaunay_triangulation_2<K, Tds>                    Delaunay;
typedef K::Point_2                                                Point2d;


#include <CGAL/Simple_cartesian.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_triangle_primitive.h>
typedef CGAL::Simple_cartesian<double> K_surface;
typedef K_surface::FT FT;
typedef K_surface::Ray_3 Ray;
typedef K_surface::Line_3 Line;
typedef K_surface::Point_3 Point3d;
typedef K_surface::Triangle_3 Triangle;
typedef std::list<Triangle>::iterator Iterator;
typedef CGAL::AABB_triangle_primitive<K_surface, Iterator> Primitive;
typedef CGAL::AABB_traits<K_surface, Primitive> AABB_triangle_traits;
typedef CGAL::AABB_tree<AABB_triangle_traits> Tree;


class Gui {
public:
	static bool redraw();
	static void init();
	static void release();
	static void update_draw_parameters(const std::list<Eigen::Vector3d> & trajectory, const Eigen::Vector4d & orientation, const Eigen::Matrix3d & position_orientation_confidence, std::vector<Point3d> (& XYZs)[3], Delaunay & triangulation, Point3d & closest_point);

private:
	static boost::mutex lock_;

	static Arcball arcball_;
	static GLfloat zoom_;
	static GLboolean is_rotating_;
	static GLboolean should_draw_surface_;
	static GLboolean should_draw_depth_;
	static GLboolean should_draw_closest_point_;
	static GLboolean should_draw_trajectory_;
	static Eigen::Vector3d model_displacement_;
	static Eigen::Vector4d model_orientation_;

	static Delaunay triangulation_;
	static Point3d closest_point_;

	static GLFWwindow* window_;

	static std::list<Eigen::Vector3d> trajectory_;
	static Eigen::Matrix3d axes_orientation_and_confidence_;
	static std::vector<Point3d> XYZs_[3];// for positions of 'mu', 'close' and 'far'


	static void draw_closest_point();
	static void draw_trajectory();
	static void draw_drone();
	static void draw_depth();
	static void draw_surface();
	static void error_callback(int error, const char* description);
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
	static void cursor_position_callback(GLFWwindow* window, double x, double y);
	static void scroll_callback(GLFWwindow* window, double x, double y);
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
};

#endif
