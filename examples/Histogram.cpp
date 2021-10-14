#include "Histogram.h"

Histogram::Histogram(std::vector<std::vector<float> > &voxels,
			  uint32_t _ch_index_0, uint32_t _ch_index_1)
{
  voxels_ptr = &voxels;
  ch_index_0 = _ch_index_0;
  ch_index_1 = _ch_index_1;
  ratio = 0.5;
}

void Histogram::makeImage()
{
  
  float image_f[HistImageWidth][HistImageHeight];
  
  for (uint32_t i = 0; i < HistImageWidth; i++) {
      for (uint32_t j = 0; j < HistImageHeight; j++) {
	image[i][j][3] = (GLubyte) 255;
	image_f[i][j] = 0;
      }
  }

  // make a 2d histogram
  float range0[2], range1[2];
  range0[0] = (*voxels_ptr)[ch_index_0][0]; range0[1] = (*voxels_ptr)[ch_index_0][0];
  range1[0] = (*voxels_ptr)[ch_index_1][0]; range0[0] = (*voxels_ptr)[ch_index_1][0];
  
   for (uint32_t j = 0; j < (*voxels_ptr)[0].size(); j++) {
     range0[0] = std::min(range0[0], (*voxels_ptr)[ch_index_0][j]);
     range1[0] = std::min(range1[0], (*voxels_ptr)[ch_index_1][j]);
     range0[1] = std::max(range0[1], (*voxels_ptr)[ch_index_0][j]);
     range1[1] = std::max(range1[1], (*voxels_ptr)[ch_index_1][j]);
   }

   for (uint32_t j = 0; j < (*voxels_ptr)[0].size(); j++) {
     float val0 = (*voxels_ptr)[ch_index_0][j];
     float val1 = (*voxels_ptr)[ch_index_1][j];
     uint32_t index_w = (val0 - range0[0])
       / ((range0[1] - range0[0])/HistImageWidth );
     uint32_t index_h = (val1 - range1[0])
       / ((range1[1] - range1[0])/HistImageHeight);

     /*if (index_h + index_w != 0)*/{
       index_h = std::min(index_h, uint32_t(HistImageHeight-1));
       index_w = std::min(index_w, uint32_t(HistImageWidth-1));
       image_f[index_w][index_h] += 1.f;
     }
   }

   float maxCount = 0;

   for (uint32_t i = 0; i < HistImageWidth; i++) {
     for (uint32_t j = 0; j < HistImageHeight; j++) {
       maxCount = std::max(maxCount, image_f[i][j]);
     }
   }

   // hack here
   maxCount = 1000;

   for (uint32_t i = 0; i < HistImageWidth; i++) {
     for (uint32_t j = 0; j < HistImageHeight; j++) {
       uint32_t c = std::min(image_f[i][j] / maxCount *255.f, 255.0f);
       if (c == 0) {
	 image[i][j][0] = (GLubyte)100;
	 image[i][j][1] = (GLubyte)100;
	 image[i][j][2] = (GLubyte)100;
	 continue;
       }
       image[i][j][0] = (GLubyte) c;
       image[i][j][1] = (GLubyte) 0;
       image[i][j][2] = (GLubyte) (255 - c);
     }
     //std::cout <<"\n";
   }
   //std::cout <<"\n";

   

   this->ch_index_0 = ch_index_0;
   this->ch_index_1 = ch_index_1;
}

void Histogram::createImageTexture(){
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glGenTextures(1, &texName);
  glBindTexture(GL_TEXTURE_2D, texName);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
		  GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
		  GL_NEAREST);

  // flip w h cuase opengl is col major
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HistImageHeight, 
	       HistImageWidth, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       image);
}


void Histogram::recreateImageTexture(){
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HistImageHeight, 
	       HistImageWidth, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       image);
}
