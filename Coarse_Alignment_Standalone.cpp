/*******************************************************************************
------------------------------------------------------------------------
Program  :  Coarse_Alignment.cpp
Purpose  :  PCL Functionalities
Author   :  Marimuthu, Prasanna
Date     :	17.09.2018

*******************************************************************************/

#include <limits>
#include <fstream>
#include <vector>
#include <iostream>
#include <ctime>
#include <Eigen/Core>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/fpfh.h>
#include <pcl/registration/ia_ransac.h>
#include <pcl/registration/icp.h>

//KDTree
#include <pcl/kdtree/flann.h>
#include <pcl/surface/gp3.h>

//MLS
#include <pcl/surface/mls.h>

//Filter
#include <pcl\PCLPointCloud2.h>
#include <pcl\segmentation\sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl\common\time.h>
#include <pcl/registration/transformation_estimation_svd.h>
#include <pcl\filters\statistical_outlier_removal.h>
#include <pcl\filters\radius_outlier_removal.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl\keypoints\harris_3d.h>

//sift
#include <pcl/common/io.h>
#include <pcl/keypoints/sift_keypoint.h>


//Functions

//function to read matrix from given file path
Eigen::Matrix4f getTransformationMatrixFn(const std::string & filePath) {

	std::ifstream Pathfile;
	// Set exceptions to be thrown on failure
	//Pathfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	Pathfile.exceptions(std::ifstream::failbit);
	try
	{
		Pathfile.open(filePath);
	}
	catch (std::system_error& exceptionArised)
	{
		std::cerr << "Could not read " << filePath << std::endl;
		std::cout << "Press ENTER to continue..."; //So the User knows what to do
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		exit(0);
	}

	Eigen::Matrix4f transMat(4, 4);
	int i, j;
	char cstring[256];
	char * split;
	for (int i = 0; i<4; i++)
	{
		Pathfile.getline(cstring, sizeof(cstring));
		split = strtok(cstring, " ");
		j = 0;
		while (split != NULL) {
			transMat(i, j) = atof(split);
			j++;
			split = strtok(NULL, " ");
		}
	}
	Pathfile.close();
	return transMat;
}

//funtion to perform transformation on a point cloud
void do_transformationFn(pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd, const std::string & filePath, pcl::PointCloud<pcl::PointXYZ>::Ptr trans_pcd) {
	
	//Read matrix from file
	Eigen::Matrix4f transform_mat = getTransformationMatrixFn(filePath);
	pcl::transformPointCloud(*main_pcd, *trans_pcd, transform_mat);
	pcl::io::savePCDFile("single_plane_transformed.pcd", *trans_pcd);
}

//function to estimate transformation matrix between two point clouds
void do_matrixCalculationFn(pcl::PointCloud<pcl::PointXYZ>::Ptr sr_pcd, pcl::PointCloud<pcl::PointXYZ>::Ptr tr_pcd) {

	//Transformation Estimation
	pcl::registration::TransformationEstimationSVD<pcl::PointXYZ, pcl::PointXYZ> TESVD;
	pcl::registration::TransformationEstimationSVD<pcl::PointXYZ, pcl::PointXYZ>::Matrix4 transformation2;
	TESVD.estimateRigidTransformation(*sr_pcd, *tr_pcd, transformation2);
	std::cout << "The Estimated Rotation and translation matrices(using getTransformation function) are : \n" << std::endl;
	printf("\n");
	printf(" | %6.3f %6.3f %6.3f | \n", transformation2(0, 0), transformation2(0, 1), transformation2(0, 2));
	printf("R = | %6.3f %6.3f %6.3f | \n", transformation2(1, 0), transformation2(1, 1), transformation2(1, 2));
	printf(" | %6.3f %6.3f %6.3f | \n", transformation2(2, 0), transformation2(2, 1), transformation2(2, 2));
	printf("\n");
	printf("t = < %0.3f, %0.3f, %0.3f >\n", transformation2(0, 3), transformation2(1, 3), transformation2(2, 3));
}

//function to filter point cloud
void do_filteringFn(pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd){

	//Each pass through filter is designed to the size of the robot table
	//X Axis
	const float depth_limit_end_x = 125.24;
	const float depth_limit_start_x = -50.0;
	pcl::PassThrough<pcl::PointXYZ> pass_x;
	pass_x.setInputCloud(main_pcd);
	pass_x.setFilterFieldName("x");
	pass_x.setFilterLimits(depth_limit_start_x, depth_limit_end_x);

	//Y Axis
	const float depth_limit_end_z = 690.0;
	const float depth_limit_start_z = 500.0;
	pcl::PassThrough<pcl::PointXYZ> pass_z;
	pass_z.setInputCloud(main_pcd);
	pass_z.setFilterFieldName("z");
	pass_z.setFilterLimits(depth_limit_start_z, depth_limit_end_z);
	pass_z.filter(*main_pcd);

	//Z Axis
	const float depth_limit_end_y = -125.0;
	const float depth_limit_start_y = 70.0;
	pcl::PassThrough<pcl::PointXYZ> pass_y;
	pass_y.setInputCloud(main_pcd);
	pass_y.setFilterFieldName("y");
	pass_y.setFilterLimits(depth_limit_end_y, depth_limit_start_y);
	pass_y.filter(*main_pcd);
	pcl::io::savePCDFile("Filtered_Cloud.pcd", *main_pcd);

}

//function to generate harris keypoints
void do_HarriskeypointFn(pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd){

	//Initialize harris 3D Keypoint
	pcl::HarrisKeypoint3D<pcl::PointXYZ, pcl::PointXYZI>* harris3D = new
		pcl::HarrisKeypoint3D<pcl::PointXYZ, pcl::PointXYZI>(pcl::HarrisKeypoint3D<pcl::PointXYZ, pcl::PointXYZI>::HARRIS);

	harris3D->setNonMaxSupression(false);
	harris3D->setRadius(12);
	harris3D->setInputCloud(main_pcd);
	int size_pt = 0;
	std::vector<int> pointIdxKeypoints;
	pcl::PointCloud<pcl::PointXYZI>::Ptr keypoints(new pcl::PointCloud<pcl::PointXYZI>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr key_regions(new pcl::PointCloud<pcl::PointXYZ>);
	harris3D->compute(*keypoints);
	pcl::StopWatch watch;
	pcl::console::print_highlight("Detected %zd points in %lfs\n", keypoints->size(), watch.getTimeSeconds());

	//Filtering by threshold
	float intensity_thresh = .0116f;
	pcl::io::savePCDFile("keypoints_Harris.pcd", *keypoints);
	pcl::console::print_info("Saved keypoints to keypoints_Harris.pcd\n");

	//Estimating size of point cloud
	for (size_t i = 0; i < keypoints->size(); i++)
	{
		if (keypoints->points[i].intensity >= intensity_thresh)
		{
			pointIdxKeypoints.push_back(i);
			++size_pt;
		}
	}
	key_regions->width = size_pt;
	key_regions->height = 1;
	key_regions->is_dense = false;
	key_regions->points.resize(key_regions->width * key_regions->height);
	for (size_t i = 0; i < size_pt; i++)
	{
		key_regions->points[i].x = keypoints->points[pointIdxKeypoints[i]].x;
		key_regions->points[i].y = keypoints->points[pointIdxKeypoints[i]].y;
		key_regions->points[i].z = keypoints->points[pointIdxKeypoints[i]].z;
	}
	pcl::io::savePCDFile("key_regions.pcd", *key_regions);
}

//funtion to do smoothing
void do_smoothingFn(pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd) {
	// Create a KD-Tree
	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);

	// Output has the PointNormal type in order to store the normals calculated by MLS
	pcl::PointCloud<pcl::PointNormal> Normal_point;

	// Init object (second point type is for the normals, even if unused)
	pcl::MovingLeastSquares<pcl::PointXYZ, pcl::PointNormal> MLS;

	MLS.setComputeNormals(true);

	// Set parameters
	MLS.setInputCloud(main_pcd);
	MLS.setPolynomialFit(true);
	MLS.setSearchMethod(tree);
	MLS.setSearchRadius(2.5);

	// Reconstruct
	MLS.process(Normal_point);

	// Save output
	pcl::io::savePCDFile("Smoothed_Cloud.pcd", Normal_point);
}

//function to generate sift keypoint
void do_SiftkeypointFn(pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd) {
	//SIFT parameters
	const float min_scale = 0.5f;
	const int n_octaves = 1;
	const int n_scales_per_octave = 2;
	const float min_contrast = 0.001f;

	// Estimate the normals of the cloud_xyz
	//Normal Estimation Parameters
	pcl::NormalEstimation<pcl::PointXYZ, pcl::PointNormal> Normal_Estimation;
	pcl::PointCloud<pcl::PointNormal>::Ptr cloud_normals(new pcl::PointCloud<pcl::PointNormal>);
	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree_n(new pcl::search::KdTree<pcl::PointXYZ>());
	Normal_Estimation.setInputCloud(main_pcd);
	Normal_Estimation.setSearchMethod(tree_n);
	Normal_Estimation.setRadiusSearch(1.5);
	Normal_Estimation.compute(*cloud_normals);

	// Copy the xyz info from cloud_xyz and add it to cloud_normals as the xyz field in PointNormals estimation is zero
	for (size_t i = 0; i<cloud_normals->points.size(); ++i)
	{
		cloud_normals->points[i].x = main_pcd->points[i].x;
		cloud_normals->points[i].y = main_pcd->points[i].y;
		cloud_normals->points[i].z = main_pcd->points[i].z;
	}

	// Estimate the sift interest points using normals values from xyz as the Intensity variants
	pcl::SIFTKeypoint<pcl::PointNormal, pcl::PointWithScale> sift;
	pcl::PointCloud<pcl::PointWithScale> result;
	pcl::search::KdTree<pcl::PointNormal>::Ptr tree(new pcl::search::KdTree<pcl::PointNormal>());
	sift.setSearchMethod(tree);
	sift.setScales(min_scale, n_octaves, n_scales_per_octave);
	sift.setMinimumContrast(min_contrast);
	sift.setInputCloud(cloud_normals);
	sift.compute(result);
	std::cout << "No of SIFT points in the result are " << result.points.size() << std::endl;

	// Copying the pointwithscale to pointxyz so as visualize the cloud
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_temp(new pcl::PointCloud<pcl::PointXYZ>);
	copyPointCloud(result, *cloud_temp);
	pcl::io::savePCDFile("SIFT_Keypoint.pcd", *cloud_temp);
}

//KD search tree to find exact estimate of keypoints
void do_kdtree_searchFn(pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd, pcl::PointCloud<pcl::PointXYZ>::Ptr after_removal){

	//Initialize Kdtree
	pcl::KdTreeFLANN<pcl::PointXYZ> kdtree;

	//Initialize output point cloud
	pcl::PointCloud<pcl::PointXYZ>::Ptr result_cloud(new pcl::PointCloud<pcl::PointXYZ>);

	//Initialize parameters
	Eigen::Vector4f centroid_PCD;
	pcl::PointXYZ searchPoint;
	std::vector<int> pointIdxRadiusSearch;
	std::vector<float> pointRadiusSquaredDistance;
	float KDTree_radius = 25.0f;

	//open write file - Text file (To make python reading simpler)
	ofstream Result_KeypointFile("Result_Points.txt");
	while (1)
	{
		if (after_removal->points.size() < 1)
		{
			cout << "limit break";
			break;
		}
		kdtree.setInputCloud(after_removal);

		//first point is our search point
		searchPoint.x = after_removal->points[0].x;
		searchPoint.y = after_removal->points[0].y;
		searchPoint.z = after_removal->points[0].z;

		//Code for K Nearest Neighbour

		//int K = 150;
		//std::vector<int> pointIdxNKNSearch(K);
		//std::vector<float> pointNKNSquaredDistance(K);

		//std::cout << "K nearest neighbor search at (" << searchPoint.x
		//	<< " " << searchPoint.y
		//	<< " " << searchPoint.z
		//	<< ") with K=" << K << std::endl;

		//if (kdtree.nearestKSearch(searchPoint, K, pointIdxNKNSearch, pointNKNSquaredDistance) > 0)
		//{
		//	for (size_t i = 0; i < pointIdxNKNSearch.size(); ++i)
		//		std::cout << "    " << orig_pcd->points[pointIdxNKNSearch[i]].x
		//		<< " " << orig_pcd->points[pointIdxNKNSearch[i]].y
		//		<< " " << orig_pcd->points[pointIdxNKNSearch[i]].z
		//		<< " (squared distance: " << pointNKNSquaredDistance[i] << ")" << std::endl;
		//}



		// Neighbors within radius search
		std::cout << "Neighbors within radius search at (" << searchPoint.x
			<< " " << searchPoint.y
			<< " " << searchPoint.z
			<< ") with radius=" << KDTree_radius << std::endl;

		//print points included and their distances
		if (kdtree.radiusSearch(searchPoint, KDTree_radius, pointIdxRadiusSearch, pointRadiusSquaredDistance) > 0)
		{
			for (size_t i = 0; i < pointIdxRadiusSearch.size(); ++i)
				std::cout << "    " << after_removal->points[pointIdxRadiusSearch[i]].x
				<< " " << after_removal->points[pointIdxRadiusSearch[i]].y
				<< " " << after_removal->points[pointIdxRadiusSearch[i]].z
				<< " (squared distance: " << pointRadiusSquaredDistance[i] << ")" << std::endl;
		}

		//Converting point indices to point cloud
		result_cloud->width = pointIdxRadiusSearch.size();
		result_cloud->height = 1;
		result_cloud->is_dense = false;
		result_cloud->points.resize(result_cloud->width * result_cloud->height);
		pcl::PointIndices::Ptr point_indx(new pcl::PointIndices());
		if (kdtree.radiusSearch(searchPoint, KDTree_radius, pointIdxRadiusSearch, pointRadiusSquaredDistance) > 0)
		{
			for (size_t i = 0; i < pointIdxRadiusSearch.size(); ++i)
			{
				result_cloud->points[i].x = after_removal->points[pointIdxRadiusSearch[i]].x;
				result_cloud->points[i].y = after_removal->points[pointIdxRadiusSearch[i]].y;
				result_cloud->points[i].z = after_removal->points[pointIdxRadiusSearch[i]].z;
				point_indx->indices.push_back(pointIdxRadiusSearch[i]);
			}
		}

		//Write keypoint in specific format
		pcl::compute3DCentroid(*result_cloud, centroid_PCD);

		if (Result_KeypointFile.is_open())
		{
			for (size_t i = 0; i < centroid_PCD.size(); ++i)
			{
				Result_KeypointFile << centroid_PCD[i];
				Result_KeypointFile << " ";
			}
			Result_KeypointFile << "\n";
		}

		// Extract the inliers from input point cloud and prepare it for next iteration
		pcl::ExtractIndices<pcl::PointXYZ> extract;
		pcl::PointCloud<pcl::PointXYZ>::Ptr temp_pcd(new pcl::PointCloud<pcl::PointXYZ>);
		extract.setInputCloud(after_removal);
		extract.setIndices(point_indx);
		extract.setNegative(true);
		extract.filter(*temp_pcd);
		after_removal = temp_pcd;

	}
	Result_KeypointFile.close();
}

//Function to perform fine tuning of point clouds
void do_ICPtuningFn(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source, pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_target){

	//initialize ICP algorithm and set inputs/ouputs
	pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
	icp.setInputSource(cloud_source);
	icp.setInputTarget(cloud_target);
	pcl::PointCloud<pcl::PointXYZ> Final;
	icp.align(Final);
	std::cout << "has converged:" << icp.hasConverged() << " score: " <<
	icp.getFitnessScore() << std::endl;
	std::cout << icp.getFinalTransformation() << std::endl;

	//Log icp result in txt
	ofstream icp_result("ICPresult.txt");
	if (icp_result.is_open())
	{
		icp_result << icp.getFinalTransformation();
	}
	icp_result.close();

}

int main(int argc, char **argv)
{

	//check for all parameters
	if (argc < 7)
	{
		printf("Not enough input file path given in command line argument!\n");
		return (-1);
	}

	//Initializing point clouds
	//CAMera Measurement Cloud
	pcl::PointCloud<pcl::PointXYZ>::Ptr main_pcd(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::io::loadPCDFile(argv[4], *main_pcd);

	//Keypoint Regions Source - CAD, Target - CAM
	pcl::PointCloud<pcl::PointXYZ>::Ptr source_keypoint(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr target_keypoint(new pcl::PointCloud<pcl::PointXYZ>);

	//Keypoints Extracted - CAM
	pcl::PointCloud<pcl::PointXYZ>::Ptr Keyregions_Extracted(new pcl::PointCloud<pcl::PointXYZ>);

	//Final Tuning on Cloud files
	pcl::PointCloud<pcl::PointXYZ>::Ptr CAM_Cloud(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZ>::Ptr CAD_Cloud(new pcl::PointCloud<pcl::PointXYZ>);

	//Output transformed cloud
	pcl::PointCloud<pcl::PointXYZ>::Ptr transformed_pcd(new pcl::PointCloud<pcl::PointXYZ>);

	//Read matrix
	const std::string & matPath = argv[7];

	bool exit_switch = false;
	do
	{
		cout << "\nOptions on Point Cloud Processing :";
		cout << "\n1. Do Transformation by reading matrix values";
		cout << "\n2. Do Estimation of transformation matrix with two point clouds";
		cout << "\n3. Do Filtering on point cloud";
		cout << "\n4. Do Harris 3d keypoint detection";
		cout << "\n5. Do Smoothing on Point cloud";
		cout << "\n6. Do SIFT keypoint detection";
		cout << "\n7. Do KD search tree to estimate exact keypoint";
		cout << "\n8. Do ICP to fine tune matching";
		cout << "\n9. Exit\n";
		int process_choice;
		cin >> process_choice;
		//Load main point cloud file - Camera Measurement

		switch (process_choice)
		{
		case 1:
			//transform point cloud
			do_transformationFn(main_pcd, matPath, transformed_pcd);
			break;

		case 2:
			//Calculate transformation matrix between two point cloud
			//load source keypoints and target key points
			pcl::io::loadPCDFile(argv[1], *source_keypoint);
			pcl::io::loadPCDFile(argv[2], *target_keypoint);
			do_matrixCalculationFn(source_keypoint, source_keypoint);
			break;

		case 3:
			//Clean point cloud by pass through filter
			do_filteringFn(main_pcd);
			break;

		case 4:
			//Generate Harris 3D keypoints
			do_HarriskeypointFn(main_pcd);
			break;

		case 5:
			//Do smoothing of Point Cloud
			do_smoothingFn(main_pcd);
			break;

		case 6:
			//Generate SIFT 3D keypoints
			do_SiftkeypointFn(main_pcd);
			break;

		case 7:
			//Load input point cloud
			pcl::io::loadPCDFile(argv[3], *Keyregions_Extracted);
			do_kdtree_searchFn(main_pcd, Keyregions_Extracted);
			break;

		case 8:
			//Generate SIFT 3D keypoints
			pcl::io::loadPCDFile(argv[4], *CAM_Cloud);
			pcl::io::loadPCDFile(argv[6], *CAD_Cloud);
			do_ICPtuningFn(CAD_Cloud, CAM_Cloud);
			break;

		case 9:
		default:
			cout << "\nExit";
			exit_switch = true;
			break;
		}
	} while (!exit_switch);

	return (0);
}

