// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* This is a small example tutorial how to use OSPRay in an application.
 *
 * On Linux build it in the build_directory with
 *   g++ ../apps/ospTutorial/ospTutorial.cpp -I ../ospray/include \
 *       -I ../../rkcommon -L . -lospray -Wl,-rpath,. -o ospTutorial
 * On Windows build it in the build_directory\$Configuration with
 *   cl ..\..\apps\ospTutorial\ospTutorial.cpp /EHsc -I ..\..\ospray\include ^
 *      -I ..\.. -I ..\..\..\rkcommon ospray.lib
 * Above commands assume that rkcommon is present in a directory right "next
 * to" the OSPRay directory. If this is not the case, then adjust the include
 * path (alter "-I <path/to/rkcommon>" appropriately).
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#define NOMINMAX
#include <conio.h>
#include <malloc.h>
#include <windows.h>
#else
#include <alloca.h>
#endif

#include <fstream>
//#include <vector>

#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"
#include "rkcommon/utility/SaveImage.h"
#include "ArcballCamera.h"

#include "voxelGeneration.h"
#include "TransferFunctionWidget.h"
#include "Histogram.h"

// stl
#include <random>
#include <vector>
#include <string>
#include <deque>

#define GLFW_INCLUDE_NONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>
// imgui
#include "imgui.h"
#include "imgui_impl_glfw_gl3.h"

//#define DEMO_VOL 

using namespace rkcommon;
using namespace rkcommon::math;

// image size
vec2i imgSize{400, 300};
vec2i windowSize{800,600};
unsigned int texture;
unsigned int guiTextures[128];
unsigned int guiTextureSize = 0;

GLFWwindow *glfwWindow = nullptr;

static const std::vector<std::string> tfnTypeStr = {"all channel same", "evenly spaced hue"};
static const std::vector<std::string> blendModeStr = {"add", "alpha blend", "hue preserve", "highest value dominate", "histogram weighted", "user define histogram mask"};
static const std::vector<std::string> frontBackStr = {"alpha blend"/*, "hue preserve", "highest value dominate"*/};
static std::vector<std::string> attributeStr = {};

class GLFWOSPWindow{
public:
  ospray::cpp::Camera camera{"perspective"};
  ospray::cpp::Renderer renderer{"multivariant"};
  ospray::cpp::World world;

  bool leftDown = false;
  bool rightDown = false;
  bool middleDown = false;
  vec2f previousMouse{vec2f(-1)};
  
  static GLFWOSPWindow *activeWindow;
  ospray::cpp::FrameBuffer framebuffer;
  std::unique_ptr<ArcballCamera> arcballCamera;

  int tfnType = 1;
  int blendMode = 5;
  int frontBackBlendMode = 0;
  
  // the list of render attributes 
  std::vector<int> renderAttributesData;
  // the bool list of if each attribute is selected to render
  std::deque<bool> renderAttributeSelection;
  // the list of render attributes weights, determined by histogram
  std::vector<float> renderAttributesWeights;
  
  std::vector<ospray::cpp::TransferFunction> tfns;
  std::vector<vec3f> colors;
  std::vector<float> colorIntensities; // opacity modifier
  std::vector<tfnw::TransferFunctionWidget> tfn_widgets; // store opacities only

  std::vector<std::vector<float> >* voxel_data; // pointer to voxels data
  std::vector<Histogram> histograms; 
  SegHistogram segHist;
  ospray::cpp::TransferFunction distFunc;
  tfnw::TransferFunctionWidget distFnWidget;
  
  GLFWOSPWindow(){
    activeWindow = this;
    
    /// prepare framebuffer
    auto buffers = OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM | OSP_FB_ALBEDO
      | OSP_FB_NORMAL;
    framebuffer = ospray::cpp::FrameBuffer(imgSize.x, imgSize.y, OSP_FB_RGBA32F, buffers);
    
  }
  
  void display();
  void motion(double, double);
  void mouse(int, int, int, int);
  void reshape(int, int);
    
  void setFunc(){

    glfwSetCursorPosCallback(
      glfwWindow, [](GLFWwindow *, double x, double y) {
		    activeWindow->motion(x, y);
		  });
    glfwSetFramebufferSizeCallback(
      glfwWindow, [](GLFWwindow *, int newWidth, int newHeight) {
        activeWindow->reshape(newWidth, newHeight);
      });

    
  }

  void renderNewFrame(){
    framebuffer.clear();
    // render one frame
    framebuffer.renderFrame(renderer, camera, world);
  }

  void buildUI();
};

GLFWOSPWindow *GLFWOSPWindow::activeWindow = nullptr;

void GLFWOSPWindow::display()
{ 
  
   glBindTexture(GL_TEXTURE_2D, texture);
   // render textured quad with OSPRay frame buffer contents
   
   renderNewFrame();
   auto fb = framebuffer.map(OSP_FB_COLOR);
  
   glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGBA32F,
		imgSize.x,
		imgSize.y,
		0,
		GL_RGBA,
		GL_FLOAT,
		fb);
   framebuffer.unmap(fb);
   
   
   glBegin(GL_QUADS);

   glTexCoord2f(0.f, 0.f);
   glVertex2f(0.f, 0.f);

   glTexCoord2f(0.f, 1.f);
   glVertex2f(0.f, windowSize.y);

   glTexCoord2f(1.f, 1.f);
   glVertex2f(windowSize.x, windowSize.y);

   glTexCoord2f(1.f, 0.f);
   glVertex2f(windowSize.x, 0.f);

   glEnd();
}

bool tfnTypeUI_callback(void *, int index, const char **out_text)
{
  *out_text = tfnTypeStr[index].c_str();
  return true;
}

bool blendModeUI_callback(void *, int index, const char **out_text)
{
  *out_text = blendModeStr[index].c_str();
  return true;
}

bool frontBackBlendModeUI_callback(void *, int index, const char **out_text)
{
  *out_text = frontBackStr[index].c_str();
  return true;
}

bool renderAttributeUI_callback(void *, int index, const char **out_text)
{
  *out_text = attributeStr[index].c_str();
  return true;
}


void GLFWOSPWindow::buildUI(){
  ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::Begin("Another Window", nullptr, flags);
  ImGui::Text("Hello from another window!");
  static int whichtfnType = 1;
  static int whichBlendMode = 5;
  static int whichFrontBackBlendMode = 0;
  static float ratio = 0.5;
  
  if (ImGui::Combo("tfn##whichtfnType",
		   &whichtfnType,
		   tfnTypeUI_callback,
		   nullptr,
		   tfnTypeStr.size())) {
     tfnType = whichtfnType;
     renderer.setParam("tfnType", tfnType); 
     renderer.commit();
  }

  if (ImGui::Combo("blendMode##whichBlendMode",
		   &whichBlendMode,
		   blendModeUI_callback,
		   nullptr,
		   blendModeStr.size())) {
     blendMode = whichBlendMode;
     renderer.setParam("blendMode", blendMode); 
     renderer.commit();
  }

    if (ImGui::Combo("frontBackBlendMode##whichFrontBackBlendMode",
		   &whichFrontBackBlendMode,
		   frontBackBlendModeUI_callback,
		   nullptr,
		   frontBackStr.size())) {
     frontBackBlendMode = whichFrontBackBlendMode;
     renderer.setParam("frontBackBlendMode", frontBackBlendMode); 
     renderer.commit();
    }
  
    if (ImGui::TreeNode("Transfer Function Selection"))
    {
      bool renderAttributeChanged = false;
      bool tfnsChanged = false;
      for (uint32_t n = 0; n < renderAttributesData.size(); n++)
	{
	  if(ImGui::Checkbox(std::to_string(renderAttributesData[n]).c_str(),
			     &renderAttributeSelection[n]))
	    renderAttributeChanged = true;

	  //intensity
	  if(ImGui::SliderFloat(("intensity "+std::to_string(n)).c_str(),
				&colorIntensities[n], 0.001f, 10.f)){
	    std::vector<vec3f> tmpColors;
	    std::vector<float> tmpOpacities;
	    
	    {
	      auto alphaOpacities = tfn_widgets[n].get_alpha_control_pts();
	      for (uint32_t i=0;i<alphaOpacities.size();i++){
		tmpColors.push_back(colors[n]);
		tmpOpacities.push_back(alphaOpacities[i].y * colorIntensities[n]);
	      }
	      tfn_widgets[n].setUnchanged();
	    }
	    tfns[n].setParam("color", ospray::cpp::CopiedData(tmpColors));
	    tfns[n].setParam("opacity", ospray::cpp::CopiedData(tmpOpacities));
	    tfns[n].commit();
	    
	    tfnsChanged = true;
	  }

	  
	  if (tfn_widgets[n].changed()) {
	    std::vector<vec3f> tmpColors;
	    std::vector<float> tmpOpacities;
	    auto alphaOpacities = tfn_widgets[n].get_alpha_control_pts();
	    for (uint32_t i=0;i<alphaOpacities.size();i++){
	      tmpColors.push_back(colors[n]);
	      tmpOpacities.push_back((alphaOpacities[i].y) * colorIntensities[n]);

	    }
	    tfn_widgets[n].setUnchanged();
    
	    tfns[n].setParam("color", ospray::cpp::CopiedData(tmpColors));
	    tfns[n].setParam("opacity", ospray::cpp::CopiedData(tmpOpacities));
	    tfns[n].commit();

	    tfnsChanged = true;
	  }

  
	  tfn_widgets[n].draw_ui();
	  
	  }
      if (renderAttributeChanged){
	// rebuild attribute list to render
	std::vector<int> renderAttributes;
        for (uint32_t i=0; i< renderAttributesData.size(); i++){
	  if (renderAttributeSelection[i])
	    renderAttributes.push_back(i);
	}
    
	renderer.setParam("renderAttributes", ospray::cpp::CopiedData(renderAttributes));
	renderer.commit();
      }

      if (tfnsChanged){
	renderer.setParam("transferFunctions", ospray::cpp::CopiedData(tfns)); 
	renderer.commit();
      }
      
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Histogram Selection"))
    {
      for (uint32_t n = 0; n < histograms.size(); n++){
	if ((ImGui::Combo("channel_y##whichChannel0Hist"+n,
			 (int*)(&histograms[n].ch_index_0),
			 renderAttributeUI_callback,
			 nullptr,
			 renderAttributesData.size()))
	  ||(ImGui::Combo("channel_x##whichChannel0Hist"+n,
			 (int*)(&histograms[n].ch_index_1),
			 renderAttributeUI_callback,
			 nullptr,
			  renderAttributesData.size())))
	      {
		histograms[n].makeImage();
		histograms[n].recreateImageTexture();
	      }

	// slider for ratio
	// change rendering and imgui line
	ImVec2 hImgSize(120, 120);
	ImVec2 lineEndP(0,0);
	if(ImGui::SliderFloat(("ratio y:x hist"+std::to_string(n)).c_str(),
				&ratio, 0.001f, 0.999f)){
	  histograms[n].ratio = ratio;
	  renderAttributesWeights[histograms[n].ch_index_0] = renderAttributesWeights[histograms[n].ch_index_1] / tan(histograms[n].ratio * M_PI/2);
	  renderer.setParam("renderAttributesWeights", ospray::cpp::CopiedData(renderAttributesWeights));
	  renderer.commit();
	}
	
	float img_k = hImgSize.y / hImgSize.x;
	if (histograms[n].ratio > atan(img_k)/M_PI * 2 ){
	  lineEndP.y = hImgSize.y;
	  lineEndP.x = hImgSize.y / tan(histograms[n].ratio * M_PI/2);
	}else{
	  lineEndP.x = hImgSize.x;
	  lineEndP.y = hImgSize.x * tan(histograms[n].ratio * M_PI/2);
	}
	// add histogram image 
	ImVec2 p = ImGui::GetCursorScreenPos();
	
	ImGui::Image((void*)(intptr_t)histograms[n].texName, hImgSize, ImVec2(0,0), ImVec2(1,-1));
	ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x , p.y + hImgSize.y), ImVec2(p.x + lineEndP.x, p.y+ hImgSize.y - lineEndP.y), IM_COL32(255, 255, 255, 255), 3.0f);
      }
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("External Segmentation"))
    {
      	// add histogram image 
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec2 hImgSize(120, 120);
	ImGui::Image((void*)(intptr_t)histograms[0].texName, hImgSize, ImVec2(0,0), ImVec2(1,-1));
	ImGui::SameLine(p.x + hImgSize.x);
	ImVec2 p2 = ImGui::GetCursorScreenPos();
	ImGui::Image((void*)(intptr_t)segHist.segTexName, hImgSize, ImVec2(0,0), ImVec2(1,-1));


	const ImGuiIO &io = ImGui::GetIO();
	bool clicked_on_item = false;

	if (ImGui::IsItemHovered() && (io.MouseDown[0] || io.MouseDown[1])) {
	  clicked_on_item = true;
	}

	const vec2f view_offset(p2.x, p.y);
	const vec2f view_scale(hImgSize.x, hImgSize.y);
	
	ImVec2 bbmin = ImGui::GetItemRectMin();
	ImVec2 bbmax = ImGui::GetItemRectMax();
	ImVec2 clipped_mouse_pos = ImVec2(std::min(std::max(io.MousePos.x, bbmin.x), bbmax.x),
					  std::min(std::max(io.MousePos.y, bbmin.y), bbmax.y));
	static float alpha_scaler;
	static float col1[3];
	static float col2[3];
	static float colImage[3];
	
	if (clicked_on_item) {
	  vec2f mouse_pos = (vec2f(clipped_mouse_pos.x, clipped_mouse_pos.y) - view_offset) / view_scale * vec2f(segHist.width, segHist.height);
	  mouse_pos.x = clamp(mouse_pos.x, 0.f, segHist.width+0.f);
	  mouse_pos.y = segHist.height - clamp(mouse_pos.y, 0.f, segHist.height+0.f);
	  int color_index = int(mouse_pos.y)*segHist.width*segHist.nChannels + int(mouse_pos.x)*segHist.nChannels;
	  /*std::cout << "click: "<<int(mouse_pos.x) <<" "<<int(mouse_pos.y)<<" ("
		    << int(segHist.image[color_index + 0])<<" "
		    << int(segHist.image[color_index + 1])<<" "
		    << int(segHist.image[color_index + 2])<<" )"
		    <<"\n";*/
	  col1[0] = int(segHist.segImage[color_index + 0])/255.f;
	  col1[1] = int(segHist.segImage[color_index + 1])/255.f;
	  col1[2] = int(segHist.segImage[color_index + 2])/255.f;
	  
	  colImage[0] = int(segHist.image[color_index + 0])/255.f;
	  colImage[1] = int(segHist.image[color_index + 1])/255.f;
	  colImage[2] = int(segHist.image[color_index + 2])/255.f;
  	  for (int i=0; i<3; i++)
	    col2[i] = col1[i];
	}
	
	if(ImGui::ColorEdit3("color 1", col1)){
	  for (int m =0; m <segHist.width*segHist.height; m++){
	    bool color_equal = true;
	    for (int i=0; i<3; i++){
	      if(segHist.segImage[m*segHist.nChannels+i] != int(col2[i]*255)){
		color_equal = false;
	      }
	    }
	    if (color_equal){
	      for (int i=0; i<3; i++){
		segHist.segImage[m*segHist.nChannels+i] = int(col1[i]*255);
		segHist.image[m*segHist.nChannels+i] = int(col1[i]*255);
		colImage[i] = int(segHist.image[m*segHist.nChannels+i])/255.f;
	      }
	    }
	  }
	  for (int i=0; i<3; i++)
	    col2[i] = col1[i];
	  
	  segHist.recreateImageTexture();
	  renderer.setParam("histMaskTexture", ospray::cpp::CopiedData(segHist.image));
	  renderer.commit();
	}
	
	static bool invisible;
	//std::cout << colImage[0]<<"\n";
	if ((!colImage[0]) && (!colImage[1]) && (!colImage[2])){ invisible = true;}
 	else invisible = false;
	  
	if(ImGui::Checkbox("set invisible", &invisible)){
	  if(invisible){
	    // set from visible to invisible
	    unsigned int currentCol[3] = {col1[0]*255, col1[1]*255, col1[2]*255};
	    unsigned int toCol[3] = {0 ,0, 0};
	    segHist.setOutputImageFromSegImage(currentCol, toCol);
	    for (int i=0; i<3; i++)
	      colImage[i] = 0.f;
	  }else{
	    unsigned int currentCol[3] = {col1[0]*255, col1[1]*255, col1[2]*255};
	    segHist.setOutputImageFromSegImage(currentCol, currentCol);
	    for (int i=0; i<3; i++)
	      colImage[i] = col1[i];
	  }

	  segHist.recreateImageTexture();
	  renderer.setParam("histMaskTexture", ospray::cpp::CopiedData(segHist.image));
	  renderer.commit();
	}
	
	if (ImGui::SliderFloat("scale opacity", &alpha_scaler, 0.000f, 10.000f)){
	  for (int m =0; m <segHist.width*segHist.height; m++){
	    bool color_equal = true;
	    for (int i=0; i<3; i++){
	      if(segHist.segImage[m*segHist.nChannels+i] != int(col1[i]*255))
		color_equal = false;
	    }
	    if (color_equal)
	       segHist.scaleAlphaForPixel(alpha_scaler, m*segHist.nChannels);
	  }
	  segHist.recreateImageTexture();
	  renderer.setParam("histMaskTexture", ospray::cpp::CopiedData(segHist.image));
	  renderer.commit();
	}
	
	if (distFnWidget.changed()){
	  std::vector<vec3f> tmpColors;
	  std::vector<float> tmpOpacities;
	  auto alphaOpacities = distFnWidget.get_alpha_control_pts();
	  auto p0 = alphaOpacities[0];
	  auto p1 = alphaOpacities[1];
	  uint32_t current_interval_start = 0;
	  uint32_t res = 255;
	  for (uint32_t i=0;i<res;i++){
	    if (i > alphaOpacities[current_interval_start+1].x*res)
	      current_interval_start++;
	    p0 = alphaOpacities[current_interval_start];
	    p1 = alphaOpacities[current_interval_start+1];
	    float current_x = i/float(res);
	    float current_val = p0.y + (current_x - p0.x)/(p1.x - p0.x)*(p1.y - p0.y);
	    tmpColors.push_back(vec3f(1,1,1));
	    tmpOpacities.push_back(current_val);
	  }
	  distFnWidget.setUnchanged();
	  
	  distFunc.setParam("color", ospray::cpp::CopiedData(tmpColors));
	  distFunc.setParam("opacity", ospray::cpp::CopiedData(tmpOpacities));
	  distFunc.commit();
	  renderer.setParam("distanceFunction", distFunc);
	  renderer.commit(); 
	}
        

	distFnWidget.draw_ui();

	ImGui::TreePop();
    }
 
  ImGui::End();

}

void GLFWOSPWindow::motion(double x, double y)
{
  const vec2f mouse(x, y);
  if (previousMouse != vec2f(-1)) {
    const bool leftDown =
      glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool rightDown =
      glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool middleDown =
      glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    const vec2f prev = previousMouse;

    bool cameraChanged = leftDown || rightDown || middleDown;

    // don't modify camera with mouse on ui window
    if (ImGui::GetIO().WantCaptureMouse) return;
    
    if (leftDown) {
      const vec2f mouseFrom(clamp(prev.x * 2.f / windowSize.x - 1.f, -1.f, 1.f),
			    clamp(prev.y * 2.f / windowSize.y - 1.f, -1.f, 1.f));
      const vec2f mouseTo(clamp(mouse.x * 2.f / windowSize.x - 1.f, -1.f, 1.f),
			  clamp(mouse.y * 2.f / windowSize.y - 1.f, -1.f, 1.f));
      arcballCamera->rotate(mouseFrom, mouseTo);
    } else if (rightDown) {
      arcballCamera->zoom(mouse.y - prev.y);
    } else if (middleDown) {
      arcballCamera->pan(vec2f(mouse.x - prev.x, prev.y - mouse.y));
    }

    if (cameraChanged) {
      //updateCamera();
      //addObjectToCommit(camera.handle());
      camera.setParam("aspect", windowSize.x / float(windowSize.y));
      camera.setParam("position", arcballCamera->eyePos());
      camera.setParam("direction", arcballCamera->lookDir());
      camera.setParam("up", arcballCamera->upDir());
      camera.commit();
    }
  }

  previousMouse = mouse;

}



void GLFWOSPWindow::reshape(int w, int h)
{
  
  windowSize.x = w;
  windowSize.y = h;
  
  // create new frame buffer
  auto buffers = OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM | OSP_FB_ALBEDO
      | OSP_FB_NORMAL;
  framebuffer =
    ospray::cpp::FrameBuffer(imgSize.x, imgSize.y, OSP_FB_RGBA32F, buffers);
  framebuffer.commit();
  
  glViewport(0, 0, windowSize.x, windowSize.y);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, windowSize.x, 0.0, windowSize.y, -1.0, 1.0);

  // update camera
  arcballCamera->updateWindowSize(windowSize);

  camera.setParam("aspect", windowSize.x / float(windowSize.y));
  camera.commit();

}


void init (void* fb){
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glEnable( GL_BLEND );
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load and generate the texture

    glTexImage2D(GL_TEXTURE_2D,
		 0,
		 GL_RGBA32F,
		 imgSize.x,
		 imgSize.y,
		 0,
		 GL_RGBA,
		 GL_FLOAT,
		 fb);

}


ospray::cpp::TransferFunction makeTransferFunction(const vec2f &valueRange)
{
  ospray::cpp::TransferFunction transferFunction("piecewiseLinear");
  std::string tfColorMap{"rgb"};
  std::string tfOpacityMap{"linear"};
  
  std::vector<vec3f> colors;
  std::vector<float> opacities;

  if (tfColorMap == "jet") {
    colors.emplace_back(0, 0, 0.562493);
    colors.emplace_back(0, 0, 1);
    colors.emplace_back(0, 1, 1);
    colors.emplace_back(0.500008, 1, 0.500008);
    colors.emplace_back(1, 1, 0);
    colors.emplace_back(1, 0, 0);
    colors.emplace_back(0.500008, 0, 0);
  } else if (tfColorMap == "rgb") {
    colors.emplace_back(0, 0, 1);
    colors.emplace_back(0, 1, 0);
    colors.emplace_back(1, 0, 0);
  } else {
    colors.emplace_back(0.f, 0.f, 0.f);
    colors.emplace_back(1.f, 1.f, 1.f);
  }

  if (tfOpacityMap == "linear") {
    opacities.emplace_back(0.f);
    opacities.emplace_back(1.f);
  } else if (tfOpacityMap == "linearInv") {
    opacities.emplace_back(1.f);
    opacities.emplace_back(0.f);
  } else if (tfOpacityMap == "opaque") {
    opacities.emplace_back(1.f);
  }

  transferFunction.setParam("color", ospray::cpp::CopiedData(colors));
  transferFunction.setParam("opacity", ospray::cpp::CopiedData(opacities));
  transferFunction.setParam("valueRange", valueRange);
  transferFunction.commit();

  return transferFunction;
}

ospray::cpp::TransferFunction makeTransferFunctionForColor(const vec2f &valueRange,
							   const vec3f &color)
{
  ospray::cpp::TransferFunction transferFunction("piecewiseLinear");
  std::string tfColorMap{"rgb"};
  std::string tfOpacityMap{"linear"};
  
  std::vector<vec3f> colors;
  std::vector<float> opacities;


  //colors.emplace_back(0.f, 0.f, 0.f);
  colors.emplace_back(color[0], color[1], color[2]);
  colors.emplace_back(color[0], color[1], color[2]);
  

    opacities.emplace_back(0.f);
    opacities.emplace_back(1.f);

  transferFunction.setParam("color", ospray::cpp::CopiedData(colors));
  transferFunction.setParam("opacity", ospray::cpp::CopiedData(opacities));
  transferFunction.setParam("valueRange", valueRange);
  transferFunction.commit();

  return transferFunction;
}




int main(int argc, const char **argv)
{
  
  // camera
  vec3f cam_pos{0.f, 0.f, 3.f};
  vec3f cam_up{0.f, 1.f, 0.f};
  vec3f cam_view{0.f, 0.f, -1.f};

#ifdef _WIN32
  bool waitForKey = false;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    // detect standalone console: cursor at (0,0)?
    waitForKey = csbi.dwCursorPosition.X == 0 && csbi.dwCursorPosition.Y == 0;
  }
#endif

  // initialize OSPRay; OSPRay parses (and removes) its commandline parameters,
  // e.g. "--osp:debug"
  OSPError init_error = ospInit(&argc, argv);
  if (init_error != OSP_NO_ERROR)
    return init_error;

  ospLoadModule("multivariant_renderer");
  if (argc < 5) {
      ::std::cerr << "Usage: " << argv[0] << "<filename> x y z n_of_channels\n";
      return 1;
  }
  
  // use scoped lifetimes of wrappers to release everything before ospShutdown()
  {

    
    // initialize GLFW
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW!");
    }

    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    // create GLFW window
    glfwWindow = glfwCreateWindow(windowSize.x, windowSize.y, "Multichannel viewer", nullptr, nullptr);

    if (!glfwWindow) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window!");
    }
    // make the window's context current
    glfwMakeContextCurrent(glfwWindow);

    
    GLFWOSPWindow glfwOspWindow;

    // create and setup model and mesh
    uint32_t n_of_ch = 1;

#ifndef DEMO_VOL
    vec3i volumeDimensions(std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]));
    n_of_ch = std::stoi(argv[5]);
#else
    vec3i volumeDimensions(100, 100, 100);
    std::vector<std::vector<float> > voxels = generateVoxels_3ch(volumeDimensions, 10);
    n_of_ch = 3;
#endif
    //std::vector<std::vector<float> > voxels = generateVoxels_nch(volumeDimensions, numPoints, 3);
    //std::cout << voxels.size()<<" channels "
    //	      << volumeDimensions.x <<"x"
    //	      << volumeDimensions.y <<"x"
    //	      << volumeDimensions.z <<" "
    //	      << voxels[0].size()<< " total voxels" << std::endl;
    
    std::vector<ospray::cpp::SharedData> voxel_data;
    //glfwOspWindow.colors.resize(n_of_ch); //= {vec3f(1,0,0), vec3f(0,1,0),vec3f(0,0,1)};
    for (uint32_t i=0; i< n_of_ch; i++){
      // pick value on rgb hue
      // 0-1  r-g, 1-2 g-b, 2-3 b-r
      float hueVal = i *3.0f/n_of_ch;
      if (hueVal < 1){
	glfwOspWindow.colors.push_back(vec3f(1-hueVal, hueVal, 0));
      }else if(hueVal < 2){
	glfwOspWindow.colors.push_back(vec3f(0, 2-hueVal, hueVal-1));
      }else{
	glfwOspWindow.colors.push_back(vec3f(hueVal-2, 0, 3-hueVal));
      }
    }
    
    std::fstream file;
    file.open(argv[1], std::fstream::in | std::fstream::binary);
    std::cout <<"dim "<<argv[2]<<" "<<argv[3]<<" "<<argv[4]<<"\n";
    std::vector<std::vector<float> > voxels_read;
    voxels_read.resize(n_of_ch);

    for (uint32_t j =0; j<n_of_ch; j++){
      float min=math::inf, max=0;
      for(uint32_t i=0; i<volumeDimensions.long_product();i++){

#ifndef DEMO_VOL
	float buff;
	//uint16_t buff;
	file.read((char*)(&buff), sizeof(buff));
	voxels_read[j].push_back(float(buff));
	if (float(buff) > max) max = float(buff);
	if (float(buff) < min) min = float(buff);
	//max = 2000;
#else
	voxels_read[j].push_back(voxels[j][i]);
	if (voxels[j][i] > max) max = voxels[j][i];
	if (voxels[j][i] < min) min = voxels[j][i];
	//std::cout << voxels[j][i];
#endif
      }
      glfwOspWindow.tfns.push_back(makeTransferFunctionForColor(vec2f(min, max), glfwOspWindow.colors[j]));
    }
    
    file.close();

    std::cout << voxels_read.size() << "x"<<voxels_read[0].size() <<" voxels read in" <<std::endl;
    glfwOspWindow.voxel_data = &voxels_read;
    
    for (const auto &v : voxels_read) {
      voxel_data.push_back(ospray::cpp::SharedData(v.data(), volumeDimensions));
    }

    
    ospray::cpp::Volume volume("structuredRegular");
    volume.setParam("gridOrigin", vec3f(-1.f));
    volume.setParam("gridSpacing", vec3f(2.f / reduce_max(volumeDimensions)));
    volume.setParam("data", ospray::cpp::SharedData(voxel_data));
    volume.setParam("dimensions", volumeDimensions);
    volume.commit();
    // put the mesh into a model
    ospray::cpp::VolumetricModel model(volume);
    model.setParam("transferFunction", glfwOspWindow.tfns[0]);
    model.commit();
    
    // put the model into a group (collection of models)
    ospray::cpp::Group group;
    group.setParam("volume", ospray::cpp::CopiedData(model));
    group.commit();

    // put the group into an instance (give the group a world transform)
    ospray::cpp::Instance instance(group);
    instance.commit();

    // put the instance in the world
    //ospray::cpp::World world;
    glfwOspWindow.world.setParam("instance", ospray::cpp::CopiedData(instance));

    // create and setup light for Ambient Occlusion
    ospray::cpp::Light light("ambient");
    light.commit();

    glfwOspWindow.world.setParam("light", ospray::cpp::CopiedData(light));
    glfwOspWindow.world.commit();

    // create renderer, choose Scientific Visualization renderer
    // load multivariant renderer module
    ospray::cpp::Renderer *renderer = &glfwOspWindow.renderer;
    //ospray::cpp::Renderer renderer("scivis");
    std::cout << "using multivariant renderer \n";

    // fill in attribute render array
    // render all attributes here
    for (uint32_t i=0; i< voxels_read.size(); i++){
      glfwOspWindow.renderAttributesData.push_back(i);
      glfwOspWindow.renderAttributeSelection.push_back(true);
      glfwOspWindow.colorIntensities.push_back(1);
      attributeStr.push_back(std::to_string(i));
      glfwOspWindow.renderAttributesWeights.push_back(1.f);
      tfnw::TransferFunctionWidget tmp;
      tmp.setGuiText("colormap " +std::to_string(i));
      glfwOspWindow.tfn_widgets.push_back(tmp);
    }
    
    glfwOspWindow.distFnWidget.setGuiText("distance function");

    //set histogram texture        
    Histogram h(voxels_read, 0, 0);
    if (n_of_ch > 1) h.ch_index_1 = 1;
    h.makeImage();
    h.createImageTexture();
    glfwOspWindow.histograms.push_back(h);
 
    glfwOspWindow.segHist.loadImage("/home/xuanhuang/Desktop/_100_100_8_segs.png");
    glfwOspWindow.segHist.loadDistImage("/home/xuanhuang/Desktop/_100_100_8_segs_dist.png");
    glfwOspWindow.segHist.applyDistAsAlpha();
    glfwOspWindow.segHist.createImageTexture();
    glfwOspWindow.segHist.createDistImageTexture();
    glfwOspWindow.distFunc = makeTransferFunctionForColor(vec2f(0.f, 1.f), vec3f(1,1,1));

    
    // complete setup of renderer
    renderer->setParam("aoSamples", 1);
    renderer->setParam("backgroundColor", 0.f); // white, transparent
    renderer->setParam("blendMode", glfwOspWindow.blendMode); // 0:add color 1: alpha blend
    renderer->setParam("renderAttributes", ospray::cpp::CopiedData(glfwOspWindow.renderAttributesData));
    renderer->setParam("renderAttributesWeights", ospray::cpp::CopiedData(glfwOspWindow.renderAttributesWeights));

    renderer->setParam("histMaskTexture", ospray::cpp::CopiedData(glfwOspWindow.segHist.image));
    
    renderer->setParam("numAttributes", voxels_read.size());
    renderer->setParam("tfnType", glfwOspWindow.tfnType); // 0:same tfn all channel 1: pick evenly on hue
    renderer->setParam("transferFunctions", ospray::cpp::CopiedData(glfwOspWindow.tfns));
    renderer->setParam("distanceFunction", glfwOspWindow.distFunc);
    renderer->commit();

    // create and setup camera
    
    // set up arcball camera for ospray
    glfwOspWindow.arcballCamera.reset(new ArcballCamera(glfwOspWindow.world.getBounds<box3f>(), windowSize));
    
    glfwOspWindow.arcballCamera->updateWindowSize(windowSize);
    
    ospray::cpp::Camera* camera = &glfwOspWindow.camera;
    camera->setParam("aspect", imgSize.x / (float)imgSize.y);
    camera->setParam("position", glfwOspWindow.arcballCamera->eyePos());
    camera->setParam("direction", glfwOspWindow.arcballCamera->lookDir());
    camera->setParam("up", glfwOspWindow.arcballCamera->upDir());
    camera->commit(); // commit each object to indicate modifications are done

    
    glfwOspWindow.renderNewFrame();


    
    ImGui_ImplGlfwGL3_Init(glfwWindow, true);
    ImGui::StyleColorsDark();
    
    auto fb = glfwOspWindow.framebuffer.map(OSP_FB_COLOR);
    init(fb);
    glfwOspWindow.framebuffer.unmap(fb);
    glfwOspWindow.setFunc();
    glfwOspWindow.reshape(windowSize.x, windowSize.y);

    
    glfwSetInputMode(glfwWindow, GLFW_STICKY_KEYS, GL_TRUE);

    
    do{
      
      
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glEnable(GL_TEXTURE_2D);
      
      ImGui_ImplGlfwGL3_NewFrame();
      
      //glBindTexture(GL_TEXTURE_2D, h.texName);
      glfwOspWindow.buildUI();
      
      glEnable(GL_FRAMEBUFFER_SRGB); // Turn on sRGB conversion for OSPRay frame
      glfwOspWindow.display();
      glDisable(GL_FRAMEBUFFER_SRGB); // Disable SRGB conversion for UI
      
      ImGui::Render();
      ImGui_ImplGlfwGL3_Render();

      //glfwOspWindow.segHist.recreateImageTexture();
      glBindTexture(GL_TEXTURE_2D, glfwOspWindow.segHist.segTexName);
      glBegin(GL_QUADS);
      glTexCoord2f(0.0, 0.0); glVertex3f(0.0, 0.0, 0.0);
      glTexCoord2f(1.0, 0.0); glVertex3f(100.0, 0.0, 0.0);
      glTexCoord2f(1.0, 1.0); glVertex3f(100.0, 100.0, 0.0);
      glTexCoord2f(0.0, 1.0); glVertex3f(0.0, 100.0, 0.0);
      glEnd();

      glBindTexture(GL_TEXTURE_2D, glfwOspWindow.segHist.distTexName);
      glBegin(GL_QUADS);
      glTexCoord2f(0.0, 0.0); glVertex3f(100.0, 0.0, 0.0);
      glTexCoord2f(1.0, 0.0); glVertex3f(200.0, 0.0, 0.0);
      glTexCoord2f(1.0, 1.0); glVertex3f(200.0, 100.0, 0.0);
      glTexCoord2f(0.0, 1.0); glVertex3f(100.0, 100.0, 0.0);
      glEnd();

      
      
      glDisable(GL_TEXTURE_2D);
      // Swap buffers
      glfwSwapBuffers(glfwWindow);
      
      glfwPollEvents();

    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
	   glfwWindowShouldClose(glfwWindow) == 0 );
    
   ImGui_ImplGlfwGL3_Shutdown();
   glfwTerminate();
   // rkcommon::utility::writePPM(
   //     "mtv_accumulatedFrameCpp.ppm", imgSize.x, imgSize.y, fb);

  }

  ospShutdown();

#ifdef _WIN32
  if (waitForKey) {
    printf("\n\tpress any key to exit");
    _getch();
  }
#endif

  return 0;
}



