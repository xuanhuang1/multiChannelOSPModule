
#include "rkcommon/utility/random.h"
#include "rkcommon/tasking/parallel_for.h"
#include "rkcommon/containers/TransactionalBuffer.h"

// stl
#include <random>

#include "voxelGeneration.h"

using namespace rkcommon;
using namespace rkcommon::math;



std::vector<float> generateVoxels(vec3i volumeDimensions,
				  int numPoints)
{
  struct Point
  {
    vec3f center;
    float weight;
  };

  // create random number distributions for point center and weight
  std::mt19937 gen(0);

  utility::uniform_real_distribution<float> centerDistribution(-1.f, 1.f);
  utility::uniform_real_distribution<float> weightDistribution(0.1f, 0.3f);

  // populate the points
  std::vector<Point> points(numPoints);

  for (auto &p : points) {
    p.center.x = centerDistribution(gen);
    p.center.y = centerDistribution(gen);
    p.center.z = centerDistribution(gen);

    p.weight = weightDistribution(gen);
  }

  // get world coordinate in [-1.f, 1.f] from logical coordinates in [0,
  // volumeDimension)
  auto logicalToWorldCoordinates = [&](int i, int j, int k) {
				     return vec3f(-1.f + float(i) / float(volumeDimensions.x - 1) * 2.f,
						  -1.f + float(j) / float(volumeDimensions.y - 1) * 2.f,
						  -1.f + float(k) / float(volumeDimensions.z - 1) * 2.f);
				   };

  // generate voxels
  std::vector<float> voxels(volumeDimensions.long_product());

  tasking::parallel_for(volumeDimensions.z,
			[&](int k)
			{
			  for (int j = 0; j < volumeDimensions.y; j++) {
			    for (int i = 0; i < volumeDimensions.x; i++) {
			      // index in array
			      size_t index = size_t(k) * volumeDimensions.x * volumeDimensions.y
				+ size_t(j) * volumeDimensions.x + size_t(i);

			      // compute volume value
			      float value = 0.f;

			      for (auto &p : points) {
				vec3f pointCoordinate = logicalToWorldCoordinates(i, j, k);
				const float distance = length(pointCoordinate - p.center);

				// contribution proportional to weighted inverse-square distance
				// (i.e. gravity)
				value += p.weight / (distance * distance);
			      }

			      voxels[index] = value;
			    }
			  }
			});
  return voxels;

}

std::vector<float> generateVoxels_center_1ch(vec3i volumeDimensions,
					     int numPoints,
					     vec3f p_center,
					     float weight){
    
  // get world coordinate in [-1.f, 1.f] from logical coordinates in [0,
  // volumeDimension)
  auto logicalToWorldCoordinates = [&](int i, int j, int k) {
				     return vec3f(-1.f + float(i) / float(volumeDimensions.x - 1) * 2.f,
						  -1.f + float(j) / float(volumeDimensions.y - 1) * 2.f,
						  -1.f + float(k) / float(volumeDimensions.z - 1) * 2.f);
				   };

  // generate voxels
  std::vector<float> voxels(volumeDimensions.long_product());

  tasking::parallel_for(volumeDimensions.z,
			[&](int k)
			{
			  for (int j = 0; j < volumeDimensions.y; j++) {
			    for (int i = 0; i < volumeDimensions.x; i++) {
			      // index in array
			      size_t index = size_t(k) * volumeDimensions.x * volumeDimensions.y
				+ size_t(j) * volumeDimensions.x + size_t(i);

			      // compute volume value
			      float value = 0.f;

			      vec3f pointCoordinate = logicalToWorldCoordinates(i, j, k);
			      const float distance = length(pointCoordinate - p_center);
			      //const float x_distance = abs(pointCoordinate[0] - p_center[0]);
			      // contribution proportional to weighted inverse-square distance
			      // (i.e. gravity)
			      //value += 0.2f / (distance * distance);
			      float r_sq = .25f;
			      if (distance*distance < r_sq)
				value = 1.f*(1-distance*distance/r_sq)*weight;
			      //value = (1 - x_distance/0.5f)*weight;
			      voxels[index] = value;
			    }
			  }
			});
  return voxels;


}


std::vector<std::vector<float> > generateVoxels_3ch(vec3i volumeDimensions,
				      int numPoints)
{
  std::vector<std::vector<float> > voxels_list;

  voxels_list.push_back(generateVoxels_center_1ch(volumeDimensions, numPoints, vec3f(-.1f, 0, 0), 1.f));
  voxels_list.push_back(generateVoxels_center_1ch(volumeDimensions, numPoints, vec3f(0, .5f, 0), 1.f));
  voxels_list.push_back(generateVoxels_center_1ch(volumeDimensions, numPoints, vec3f(.1f, 0, 0), 1.f));  
  return voxels_list;
}

std::vector<std::vector<float> > generateVoxels_nch(vec3i volumeDimensions,
						    int numPoints,
						    int n)
{
  srand (time(NULL));
  std::vector<std::vector<float> > voxels_list;

  for (int i=0; i<n; i++){
    vec3f randCenter((rand()%100)/100.f - 0.5f,
		     (rand()%100)/100.f - 0.5f,
		     (rand()%100)/100.f - 0.5f);
    voxels_list.push_back(generateVoxels_center_1ch(volumeDimensions,
						    numPoints,
						    randCenter, 1.f));
  }
  return voxels_list;
}
