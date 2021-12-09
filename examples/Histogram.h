#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

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
  std::map<std::vector<int>, int> colorSegIDMap = {};
  std::vector<int> segAlphaModifier = {};
  
  SegHistogram(){};
  void loadImage(char* filename);
  void createImageTexture();
  void recreateImageTexture();
  
  void loadDistImage(char* filename);
  void createDistImageTexture(); // for display only

  void applyDistAsAlpha();
  void scaleAlphaForPixel(float scale, int col_index);
  void setOutputImageFromSegImage(unsigned int from[3], unsigned int to[3]);

  int getColorSegID(unsigned int col[3]){
    std::vector<int> color = {int(col[0]), int(col[1]), int(col[2])};
    if (colorSegIDMap.find(color) != colorSegIDMap.end() ) return colorSegIDMap[color];
    return -1;
  }

  std::vector<int> getSegColWithAlphaModifier(){
    std::vector<int> ret;
    ret.resize(4*colorSegIDMap.size());
    
    if (segAlphaModifier.size() != colorSegIDMap.size())
      std::cerr <<"seg color id and alpha arrays sizes mismatch!\n"; 
    for(auto ii=colorSegIDMap.begin(); ii!=colorSegIDMap.end(); ++ii)
    {
      // value from map is the seg index
      ret[int(ii->second)*4] = (ii->first[0]);
      ret[int(ii->second)*4 + 1] = (ii->first[1]);
      ret[int(ii->second)*4 + 2] = (ii->first[2]);
      ret[int(ii->second)*4 + 3] = segAlphaModifier[ii->second];
    }
    //for (int i=0; i<ret.size(); i++)
    //  std::cout <<ret[i] <<" ";
    //std::cout <<"\n";
    
    return ret;
  }
};
