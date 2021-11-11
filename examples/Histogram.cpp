#include "Histogram.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
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
  
  float image_f[HistImageHeight][HistImageWidth];
  
  for (uint32_t i = 0; i < HistImageHeight; i++) {
      for (uint32_t j = 0; j < HistImageWidth; j++) {
        image[i][j][3] = (GLubyte) 255;
	image_f[i][j] = 0;
      }
  }

  if(voxels_ptr == nullptr) std::cout << "no input voxel for 2d histogram\n";
  else std::cout <<"voxels data for 2d histogram dim:"<< (*voxels_ptr).size()<<"x"<<(*voxels_ptr)[0].size()<<"\n";
  
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

   std::cout << "range:"<< range0[0] <<" "<<range0[1]
	     <<"  "<<range1[0]<<" "<<range1[1]<<"\n";

   for (uint32_t j = 0; j < (*voxels_ptr)[0].size(); j++) {
     float val0 = (*voxels_ptr)[ch_index_0][j];
     float val1 = (*voxels_ptr)[ch_index_1][j];
     uint32_t index_0 = (val0 - range0[0])
       / ((range0[1] - range0[0])/HistImageHeight );
     uint32_t index_1 = (val1 - range1[0])
       / ((range1[1] - range1[0])/HistImageWidth);

     {
       index_1 = std::min(index_1, uint32_t(HistImageWidth-1));
       index_0 = std::min(index_0, uint32_t(HistImageHeight-1));
       image_f[index_0][index_1] += 1.f;
     }
   }

   float maxCount = 0;

   for (uint32_t i = 0; i < HistImageHeight; i++) {
     for (uint32_t j = 0; j < HistImageWidth; j++) {
       maxCount = std::max(maxCount, image_f[i][j]);
     }
   }

   // hack here
   maxCount = 200;

   for (uint32_t i = 0; i < HistImageHeight; i++) {
     for (uint32_t j = 0; j < HistImageWidth; j++) {
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

   //for (int i=0; i<4; i++)
   //  image[16][32][i] = 255;
   

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

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HistImageWidth, 
	       HistImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       image);
}


void Histogram::recreateImageTexture(){
  glBindTexture(GL_TEXTURE_2D, texName);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, HistImageWidth, 
	       HistImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       image);
}



void SegHistogram::loadImage(char* filename){
  int nChannels, read_nChannels;
  filename = filename;
  unsigned char* image_read = stbi_load(filename, &width, &height, &read_nChannels, 0);
  std::cout <<"mask image: "<<width <<"x"<<height <<"x"<<read_nChannels <<" \n";

  if (read_nChannels == 3) nChannels = 4;
  image.resize(width*height*nChannels);

  if (read_nChannels == 3){
    for (int i=0; i<height; i++)
      for (int j=0; j<width; j++)
	image[i*width*nChannels + j*nChannels + 3] = 255;
  }
  
  for (int i=0; i<height/2.0+1; i++){
    for (int j=0; j<width; j++){
      for (int k=0; k<read_nChannels; k++){
	image[i*width*nChannels + j*nChannels + k] = image_read[(height-1-i)*width*read_nChannels + j*read_nChannels + k];
	image[(height-1-i)*width*nChannels + j*nChannels + k] = image_read[i*width*read_nChannels + j*read_nChannels + k];
      }
    }
  }
  delete[] image_read;
  
}


void SegHistogram::createImageTexture(){
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glGenTextures(1, &texName);
  glBindTexture(GL_TEXTURE_2D, texName);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
		  GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
		  GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, 
	       height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       &image[0]);
}


void SegHistogram::recreateImageTexture(){
  glBindTexture(GL_TEXTURE_2D, texName);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width,
	       height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       &image[0]);
}

