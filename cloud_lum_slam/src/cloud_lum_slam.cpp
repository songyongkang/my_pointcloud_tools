/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2012, Jochen Sprickerhof
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */

#include <pcl/console/parse.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/common/transforms.h>
#include <pcl/registration/lum.h>
#include <pcl/registration/correspondence_estimation.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <vector>

typedef pcl::PointXYZ PointType;
typedef pcl::PointCloud<PointType> Cloud;
typedef Cloud::ConstPtr CloudConstPtr;
typedef Cloud::Ptr CloudPtr;
typedef std::pair<std::string, CloudPtr> CloudPair;
typedef std::vector<CloudPair> CloudVector;

std::vector<Eigen::Vector6f> readFromCsv(std::string filename);

int
main (int argc, char **argv)
{
  double dist = 0.1;
  pcl::console::parse_argument (argc, argv, "-d", dist);

  int iter = 3;
  pcl::console::parse_argument (argc, argv, "-i", iter);

  int lumIter = 30;
  pcl::console::parse_argument (argc, argv, "-l", lumIter);

  double loopDist = 1.0;
  pcl::console::parse_argument (argc, argv, "-D", loopDist);

  int loopCount = 9000;
  pcl::console::parse_argument (argc, argv, "-c", loopCount);

  pcl::registration::LUM<PointType> lum;
  lum.setMaxIterations (lumIter);
  lum.setConvergenceThreshold (0.001f);

  std::vector<int> pcd_indices;
  pcd_indices = pcl::console::parse_file_extension_argument (argc, argv, ".pcd");

  std::vector<int> pose_file;
  pose_file = pcl::console::parse_file_extension_argument(argc, argv, ".csv");
  std::vector<Eigen::Vector6f> poses;
  bool use_pose = false;
  if (pose_file.size() != 0) {
    poses = readFromCsv(argv[pose_file[0]]);
    use_pose = true;
  }
  
  CloudVector clouds;
  for (size_t i = 0; i < pcd_indices.size (); i++)
  {
    CloudPtr pc (new Cloud);
    pcl::io::loadPCDFile (argv[pcd_indices[i]], *pc);
    clouds.push_back (CloudPair (argv[pcd_indices[i]], pc));
    std::cout << "loading file: " << argv[pcd_indices[i]] << " size: " << pc->size () << std::endl;
    if (use_pose) {
      lum.addPointCloud(clouds[i].second, poses[i]);
      std::cout << "pose :" << poses[i] << std::endl;
    }else{
      lum.addPointCloud (clouds[i].second);
    }
  }

  for (int i = 0; i < iter; i++)
  {
    std::cout << "iter : " << i << " / " << iter << std::endl;
    for (size_t i = 1; i < clouds.size (); i++)
      for (size_t j = 0; j < i; j++)
      {
        Eigen::Vector4f ci, cj;
        if(use_pose){
          ci(0, 0) = poses[i](0, 0);
          ci(1, 0) = poses[i](1, 0);
          ci(2, 0) = poses[i](2, 0);
          ci(3, 0) = 0.0;
          cj(0, 0) = poses[j](0, 0);
          cj(1, 0) = poses[j](1, 0);
          cj(2, 0) = poses[j](2, 0);
          cj(3, 0) = 0.0;
        }else{
          pcl::compute3DCentroid (*(clouds[i].second), ci);
          pcl::compute3DCentroid (*(clouds[j].second), cj);
        }
        Eigen::Vector4f diff = ci - cj;
        //std::cout << i << " " << j << " " << diff.norm () << std::endl;
        if(diff.norm () < loopDist && (i - j == 1 || i - j > loopCount))
        {
          // if(i - j > loopCount)

          std::cout << "add connection between " << i << " (" << clouds[i].first << ") and " << j << " (" << clouds[j].first << ")" << std::endl;
          pcl::registration::CorrespondenceEstimation<PointType, PointType> ce;
          ce.setInputTarget (clouds[i].second);
          ce.setInputSource (clouds[j].second);
          pcl::CorrespondencesPtr corr (new pcl::Correspondences);
          ce.determineCorrespondences (*corr, dist);
          if (corr->size () > 2){
            lum.setCorrespondences (j, i, corr);
            //std::cout << "Set correspondences" << std::endl;
          }
        }
      }

    lum.compute ();

    for(size_t i = 0; i < lum.getNumVertices (); i++)
    {
      std::cout << i << ": " << lum.getTransformation (i) (0, 3) << " " << lum.getTransformation (i) (1, 3) << " " << lum.getTransformation (i) (2, 3) << std::endl;
      clouds[i].second = lum.getTransformedCloud (i);
    }
  }

  for(size_t i = 0; i < lum.getNumVertices (); i++)
  {
    std::string result_filename (clouds[i].first);
    result_filename = result_filename.substr (result_filename.rfind ("/") + 1);
    pcl::io::savePCDFileBinary (result_filename.c_str (), *(clouds[i].second));
    std::cout << "saving result to " << result_filename << std::endl;
  }

  return 0;
}

std::vector<Eigen::Vector6f> readFromCsv(std::string filename)
{
  std::ifstream ifs(filename.c_str());
  if(!ifs){
    std::cout << "Pose file is found in args, but it is invalid filepath." << std::endl;
    exit(-1);
  }
  std::string str;
  std::vector<Eigen::Vector6f> poses;
  while(getline(ifs, str)){
    std::string token;
    std::istringstream stream(str);
    Eigen::Vector6f pose;
    int count = 0;
    while(getline(stream, token, ',')){
      pose(count, 0) = std::stof(token);
      count++;
    }
    poses.push_back(pose);
  }
  return poses;
}

