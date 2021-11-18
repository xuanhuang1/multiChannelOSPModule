#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>


#define HistImageWidth 64
#define HistImageHeight 64

class Histogram{
public:
  GLubyte image[HistImageHeight][HistImageWidth][4];
  unsigned int texName;
  uint32_t ch_index_0;
  uint32_t ch_index_1;
  float ratio;
  std::vector<std::vector<float> >* voxels_ptr;

  Histogram(std::vector<std::vector<float> > &voxels,
	    uint32_t ch_index_0, uint32_t ch_index_1);
  void makeImage();
  void createImageTexture();
  void recreateImageTexture();
};

class SegHistogram{
public:
  std::vector<unsigned char> image;;
  unsigned int texName;
  int width;
  int height;
  int nChannels;
  char* filename;
  SegHistogram(){};
  void loadImage(char* filename);
  void createImageTexture();
  void recreateImageTexture();
};
