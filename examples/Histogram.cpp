#include "Histogram.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
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
  range1[0] = (*voxels_ptr)[ch_index_1][0]; range1[1] = (*voxels_ptr)[ch_index_1][0];
  
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

   for (uint32_t i = 0; i < HistImageHeight; i++) {
     for (uint32_t j = 0; j < HistImageWidth; j++) {
       float c_f = std::log(image_f[i][j]) / std::log(maxCount);
       uint32_t c = c_f*255.f;
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

void SegHistogram::writeImage(const char* filename){
  stbi_write_png(filename, width, height, 4, &segImage[0], width*4);
  std::cout << "wrote to image: "<< filename<<"\n";
}

void SegHistogram::loadImage(const char* filename){
  int read_nChannels;
  this->filename = filename;
  std::cout << "load image:" << this->filename<<"\n";
  
  unsigned char* image_read = stbi_load(filename, &width, &height, &read_nChannels, 0);
  std::cout <<"mask image: "<<width <<"x"<<height <<"x"<<read_nChannels <<" \n";

  nChannels = 4;
  //if (read_nChannels == 3) nChannels = 4;
  image.resize(width*height*nChannels);
  segImage.resize(width*height*nChannels);
  
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
  
  for (int i=0; i<height; i++)
    for (int j=0; j<width; j++)
      for (int k=0; k<nChannels; k++)
	segImage[i*width*nChannels + j*nChannels + k] = image[i*width*nChannels + j*nChannels + k];

  // make color segment id map
  int color_id = 0;
  colorSegIDMap.clear();
  segAlphaModifier.clear();
  for (int i=0; i<height; i++){
    for (int j=0; j<width; j++){
      std::vector<int> currect_col;
      for (int k=0; k<3; k++)
	currect_col.push_back(int(segImage[i*width*nChannels + j*nChannels + k]));
      if (colorSegIDMap.find(currect_col) == colorSegIDMap.end() ){
	colorSegIDMap.insert(std::make_pair(currect_col, color_id));
	segAlphaModifier.push_back(1);
	color_id++;
      }
    }
  }

  std::cout <<"\n segments loaded as:\n";
  for(auto ii=colorSegIDMap.begin(); ii!=colorSegIDMap.end(); ++ii)
    {
      std::cout << '{' << ii->first[0] << ", \t"
		<< ii->first[1] << ", \t"
		<< ii->first[2] << "} \t: \t" << ii->second << '\n';
    }
  std::cout <<" total "<<colorSegIDMap.size()<<  " segments\n\n";
  
  
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

  
  glGenTextures(1, &segTexName);
  glBindTexture(GL_TEXTURE_2D, segTexName);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
		  GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
		  GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, 
	       height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       &segImage[0]);
}


void SegHistogram::recreateImageTexture(){

  
  
  glBindTexture(GL_TEXTURE_2D, texName);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width,
	       height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       &image[0]);

  glBindTexture(GL_TEXTURE_2D, segTexName);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width,
         height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
  	       &segImage[0]);
}



void SegHistogram::loadDistImage(char* distFileName){
  int read_nChannels;
  int read_width, read_height, read_channel;
  unsigned char* image_read = stbi_load(distFileName, &read_width, &read_height, &read_nChannels, 0);
  if ((read_width != width) || (read_height != height)){
    // dim has to match to the segmetation image
    std::cerr << "size mismatch: "<< width <<"x"<<height <<" expected, "
	      << read_width <<"x"<<read_height<<" read\n";
    return;
  }else
    std::cout <<"dist image: "<<read_width <<"x"<<read_height <<"x"<<read_nChannels <<" \n";
  
    
  //if (read_nChannels == 3) nChannels = 4;
  distImage.resize(width*height*nChannels);

  if (read_nChannels == 3){
    for (int i=0; i<height; i++)
      for (int j=0; j<width; j++)
	distImage[i*width*nChannels + j*nChannels + 3] = 255;
  }
  
  for (int i=0; i<height/2.0+1; i++){
    for (int j=0; j<width; j++){
      for (int k=0; k<read_nChannels; k++){
	distImage[i*width*nChannels + j*nChannels + k] = image_read[(height-1-i)*width*read_nChannels + j*read_nChannels + k];
	distImage[(height-1-i)*width*nChannels + j*nChannels + k] = image_read[i*width*read_nChannels + j*read_nChannels + k];
      }
    }
  }
  
  delete[] image_read;

}

void SegHistogram::createDistImageTexture(){
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glGenTextures(1, &distTexName);
  glBindTexture(GL_TEXTURE_2D, distTexName);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
		  GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
		  GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, 
	       height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 
	       &distImage[0]);
}


void SegHistogram::applyDistAsAlpha(){
  if ((!distImage.size()) || (!image.size())){
    std::cerr << "distance or segmentation is empty!\n";
    return;
  }
  for (int i=0; i<height; i++)
      for (int j=0; j<width; j++)
	image[i*width*nChannels + j*nChannels + 3] = 255;//distImage[i*width*nChannels + j*nChannels];
  std::cout << "ignore distance image, apply uniform \n";
}


void SegHistogram::multHistAsAlpha(Histogram &hist, bool apply){
     for (int i=0; i<height; i++){
        for (int j=0; j<width; j++){
            uint32_t hist_i = (i+0.f)/height*HistImageHeight;
            uint32_t hist_j = (j+0.f)/width*HistImageWidth;
            uint32_t color_index = i*width*nChannels + j*nChannels;
            if (!apply){
                segImage[color_index + 0] = image[color_index];
                segImage[color_index + 1] = image[color_index + 1];
                segImage[color_index + 2] = image[color_index + 2];
                segImage[color_index + 3] = 255;
            }else{
                if ((hist.image[hist_i][hist_j][0] == (GLubyte)100) &&
                    (hist.image[hist_i][hist_j][1] == (GLubyte)100) &&
                    (hist.image[hist_i][hist_j][2] == (GLubyte)100)){
                    segImage[color_index + 0] = 0;
                    segImage[color_index + 1] = 0;
                    segImage[color_index + 2] = 0;
                }
                else
                {
                    //segImage[i*width*nChannels + j*nChannels + 3] =
                    //    image[i*width*nChannels + j*nChannels + 3] / 255.f * hist.image[hist_i][hist_j][0];
                }
            }
        }
    }
}


void SegHistogram::scaleAlphaForPixel(float scale, int col_index){
  if ((!distImage.size()) || (!image.size())){
    std::cerr << "distance or segmentation is empty!\n";
    return;
  }
  image[col_index + 3] = std::min(int(distImage[col_index]*scale), 255);
}


void SegHistogram::setOutputImageFromSegImage(unsigned int from[3], unsigned int to[3]){
  for (int m =0; m < width*height; m++){
    bool color_equal = true;
    for (int i=0; i<3; i++){
      if(segImage[m*nChannels+i] != from[i])
	color_equal = false;
    }
    if (color_equal){
      for (int i=0; i<3; i++)
	image[m*nChannels+i] = to[i];
    }
  }
}
