#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>


#define HistImageWidth 32
#define HistImageHeight 32

class Histogram{
public:
  GLubyte image[HistImageHeight][HistImageWidth][4];
  unsigned int texName;
  uint32_t ch_index_0;
  uint32_t ch_index_1;
  void makeImage(std::vector<std::vector<float> > &voxels,
		 uint32_t ch_index_0, uint32_t ch_index_1);

  void createImageTexture();
};
