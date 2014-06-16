
#include "ekfoa.hpp"

EKFOA::EKFOA() :
cam(Camera(
		//Sample
		0.0112,	  		    //d
		1.7945 / 0.0112,  //Cx
		1.4433 / 0.0112,  //Cy
		6.333e-2, //k1
		1.390e-2, //k2
		2.1735   			//f

		//ARDRONE:
//		0.0112,	  		    //d
//		303.4832388214409775173407979309558868408203125,                    //Cx
//		185.030127342570523296672035939991474151611328125,                  //Cy
//		0.01315450896536778969958536578133134753443300724029541015625,      //k1
//		0.000395589335442645350336687837256022248766385018825531005859375,  //k2
//		2.1735   			//f
)),
filter(Kalman(
		0.0,   //v_0
		0.025, //std_v_0
		1e-15, //w_0
		0.025, //std_w_0
		0.007, //standar deviation for linear acceleration noise
		0.007, //standar deviation for angular acceleration noise
		1.0    //standar deviation for measurement noise
)),
motion_tracker(MotionTrackerOF(
		30, //min_number_of_features_in_image
		20  //distance_between_points
)) {

}

void EKFOA::start(){
	cv::Mat frame;

	double time;

	//Sequence path and initial image
	std::string sequence_prefix = std::string(getpwuid(getuid())->pw_dir) + "/btsync/capture_samples/monoSLAM/ekfmonoslam/rawoutput";
//	std::string sequence_prefix = std::string(getpwuid(getuid())->pw_dir) + "/btsync/capture_samples/monoSLAM/ardrone/rotation_indoors2/img";
	int initIm = 90;
	int lastIm = 2000;

	char file_path[255]; // enough to hold all numbers up to 64-bits

	cv::namedWindow("Camera input", cv::WINDOW_AUTOSIZE );
	cv::moveWindow("Camera input", 1040, 0);

	for (int step=initIm+1 ; step<lastIm ; step++){
		std::cout << "step: " << step << std::endl;

		//EKF prediction (state and measurement prediction)
		double delta_t = 1; //TODO: take time delta from timestamp of when the image was taken

		time = (double)cv::getTickCount();
		filter.predict_state_and_covariance(delta_t);
		time = (double)cv::getTickCount() - time;
		std::cout << "predict = " << time/((double)cvGetTickFrequency()*1000.) << "ms" << std::endl;

		//Sense environment (process frame)
		Eigen::MatrixXd features_to_add;
		std::vector<int> features_to_remove;
		std::vector<cv::Point2f> features_tracked;
		sprintf(file_path, "%s%04d.pgm", sequence_prefix.c_str(), step);
//		sprintf(file_path, "%s%03d.png", sequence_prefix.c_str(), step);
		frame = cv::imread(file_path, CV_LOAD_IMAGE_COLOR);   // Read the file

		time = (double)cv::getTickCount();
		motion_tracker.process(frame, features_to_add, features_tracked, features_to_remove);
		//TODO: Why is optical flow returning points outside the image???
		time = (double)cv::getTickCount() - time;
		std::cout << "tracker = " << time/((double)cvGetTickFrequency()*1000.) << "ms" << std::endl;

		//Delete no longer seen features
		time = (double)cv::getTickCount();
		filter.delete_features( features_to_remove );
		time = (double)cv::getTickCount() - time;
		std::cout << "delete = " << time/((double)cvGetTickFrequency()*1000.) << "ms" << std::endl;


		//EKF Update step
		time = (double)cv::getTickCount();
		filter.update(cam, features_tracked);
		time = (double)cv::getTickCount() - time;
		std::cout << "update = " << time/((double)cvGetTickFrequency()*1000.) << "ms" << std::endl;

		//Add new features
		time = (double)cv::getTickCount();
		filter.add_features_inverse_depth( cam, features_to_add );
		time = (double)cv::getTickCount() - time;
		std::cout << "add_features = " << time/((double)cvGetTickFrequency()*1000.) << "ms" << std::endl;


		time = (double)cv::getTickCount();
		std::vector< std::pair<Point2d, size_t> > triangle_list;

		const Eigen::VectorXd & x_k_k = filter.x_k_k();
		const Eigen::MatrixXd & p_k_k = filter.p_k_k();

		std::vector<Point3d> XYZs_mu;
		std::vector<Point3d> XYZs_close;
		std::vector<Point3d> XYZs_far;

	    Eigen::Matrix3d R;
	    MotionModel::quaternion_matrix(x_k_k.segment<4>(3), R);
	    Eigen::Matrix3d R_inv = R.inverse();

		//Compute the positions and inverse depth variances of all the points in the state
		for (size_t i=0 ; i<features_tracked.size()+features_to_add.cols() ; i++){
			const int start_feature = 13 + i*6;
			const int feature_inv_depth_index = start_feature + 5;

			//As with any normal distribution, nearly all (99.73%) of the possible depths lie within three standard deviations of the mean!
			const double sigma_3 = std::sqrt(p_k_k(feature_inv_depth_index, feature_inv_depth_index)); //sqrt(depth_variance)
			const double size_sigma_3 = std::abs(1.0/(x_k_k(feature_inv_depth_index)-sigma_3) - 1.0/(x_k_k(feature_inv_depth_index)+sigma_3));

			const Eigen::VectorXd & yi = x_k_k.segment(start_feature, 6);
			Eigen::VectorXd point_close(x_k_k.segment(start_feature, 6));
			Eigen::VectorXd point_far(x_k_k.segment(start_feature, 6));

			point_close(5) += sigma_3;
			point_far(5) -= sigma_3;

			//The center of the model is ALWAYS the current position of the camera/robot, so have to 'cancel' the current orientation (R_inv) and translation (rWC = x_k_k.head(3)):
			//Note: It is nicer to do this in the GUI class, as it is only a presention/perspective change. But due to the structure, it was easier to do it here.
			Eigen::Vector3d XYZ_mu = R_inv * Feature::compute_cartesian(yi) - x_k_k.head(3); //mu (mean)
			Eigen::Vector3d XYZ_close = R_inv * Feature::compute_cartesian(point_close) - x_k_k.head(3); //mean + 3*sigma. (since inverted signs are also inverted)
			Eigen::Vector3d XYZ_far = R_inv * Feature::compute_cartesian(point_far) - x_k_k.head(3); //mean - 3*sigma


			XYZs_mu.push_back(Point3d(XYZ_mu(0), XYZ_mu(1), XYZ_mu(2)));
			XYZs_close.push_back(Point3d(XYZ_close(0), XYZ_close(1), XYZ_close(2)));
			XYZs_far.push_back(Point3d(XYZ_far(0), XYZ_far(1), XYZ_far(2)));

			//If the size that contains the 99.73% of the inverse depth distribution is smaller than the current inverse depth, add it to the surface:
			if (size_sigma_3 < 1/x_k_k(feature_inv_depth_index)){
				triangle_list.push_back( std::make_pair( Point2d(features_tracked[i].x, features_tracked[i].y), i));
			}

			if (x_k_k(feature_inv_depth_index) < 0 ){
				std::cout << "feature behind the camera!!! : idx=" << feature_inv_depth_index << ", value=" << x_k_k(feature_inv_depth_index) << std::endl;
				std::cin.ignore(1);
			}
		}

		std::list<Triangle> triangles_list_3d;
		Delaunay triangulation(triangle_list.begin(), triangle_list.end());
		cv::Scalar delaunay_color = cv::Scalar(255, 0, 0); //blue
		for(Delaunay::Finite_faces_iterator fit = triangulation.finite_faces_begin(); fit != triangulation.finite_faces_end(); ++fit) {
			const Delaunay::Face_handle & face = fit;
			//face->vertex(i)->info() = index of the point in the observation list.
			line(frame, features_tracked[face->vertex(0)->info()], features_tracked[face->vertex(1)->info()], delaunay_color, 1);
			line(frame, features_tracked[face->vertex(1)->info()], features_tracked[face->vertex(2)->info()], delaunay_color, 1);
			line(frame, features_tracked[face->vertex(2)->info()], features_tracked[face->vertex(0)->info()], delaunay_color, 1);

			//Add the face of the linked 3d points of this 2d triangle:
			triangles_list_3d.push_back(Triangle(XYZs_close[face->vertex(0)->info()], XYZs_close[face->vertex(1)->info()], XYZs_close[face->vertex(2)->info()]));
		}

		// constructs AABB tree
		Tree tree(triangles_list_3d.begin(), triangles_list_3d.end());

		Point3d closest_point;
		if (tree.size()>0){
			// compute closest point and squared distance
			Point3d point_query(x_k_k(0), x_k_k(1), x_k_k(2));
			closest_point = tree.closest_point(point_query);
			std::cerr << "closest point is: " << closest_point << std::endl;
			FT sqd = tree.squared_distance(point_query);
			std::cout << "squared distance: " << sqd << std::endl;
		}


		time = (double)cv::getTickCount() - time;
		std::cout << "obstacle avoidance = " << time/((double)cvGetTickFrequency()*1000.) << "ms" << std::endl;

		//Show input frame
		cv::imshow("Camera input", frame);

		//Notify the gui of the new state:
		Gui::update_state_and_cov(x_k_k.head<3>(), x_k_k.segment<4>(3), XYZs_mu, XYZs_close, XYZs_far, triangulation, closest_point);


		//PAUSE:
		std::cin.ignore(1);
	}
}
