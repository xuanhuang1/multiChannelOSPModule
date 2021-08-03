
#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"

std::vector<float> generateVoxels(rkcommon::math::vec3i volumeDimensions,
				  int numPoints);

std::vector<float> generateVoxels_center_1ch(rkcommon::math::vec3i volumeDimensions,
					     int numPoints,
					     rkcommon::math::vec3f p_center);

std::vector<std::vector<float> > generateVoxels_3ch(rkcommon::math::vec3i volumeDimensions,
						    int numPoints);
