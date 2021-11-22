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
  std::vector<unsigned char> image;
  std::vector<unsigned char> segImage;
  std::vector<unsigned char> distImage;
  unsigned int texName;
  unsigned int segTexName;
  unsigned int distTexName;
  int width;
  int height;
  int nChannels;
  char* filename;
  SegHistogram(){};
  void loadImage(char* filename);
  void createImageTexture();
  void recreateImageTexture();
  
  void loadDistImage(char* filename);
  void createDistImageTexture(); // for display only

  void applyDistAsAlpha();
  void scaleAlphaForPixel(float scale, int col_index);
  void setOutputImageFromSegImage(unsigned int from[3], unsigned int to[3]);
};
