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

using namespace rkcommon;
using namespace rkcommon::math;


// image size
vec2i imgSize{1024, 768};
vec2i windowSize{1024,768};
unsigned int texture;

GLFWwindow *glfwWindow = nullptr;

static const std::vector<std::string> tfnTypeStr = {"all channel same", "evenly spaced hue"};
static const std::vector<std::string> blendModeStr = {"add", "alpha blend", "hue preserve"};

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
  int blendMode = 0;
  std::vector<int> renderAttributesData;
  std::deque<bool> renderAttributeSelection;
  
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
		windowSize.x,
		windowSize.y,
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


void GLFWOSPWindow::buildUI(){
  ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::Begin("Another Window", nullptr, flags);
  ImGui::Text("Hello from another window!");
  static int whichtfnType = 1;
  static int whichBlendMode = 0;
  
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
  
  if (ImGui::TreeNode("Selection State: Multiple Selection"))
    {
      bool renderAttributeChanged = false;
      for (uint32_t n = 0; n < renderAttributesData.size(); n++)
	{
	  if(ImGui::Checkbox(std::to_string(renderAttributesData[n]).c_str(),
			     &renderAttributeSelection[n]))
	    renderAttributeChanged = true;
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
    ospray::cpp::FrameBuffer(windowSize.x, windowSize.y, OSP_FB_RGBA32F, buffers);
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

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load and generate the texture

    glTexImage2D(GL_TEXTURE_2D,
		 0,
		 GL_RGBA32F,
		 windowSize.x,
		 windowSize.y,
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


  colors.emplace_back(0.f, 0.f, 0.f);
  colors.emplace_back(color[0], color[1], color[2]);
  

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
  if (argc < 4) {
      ::std::cerr << "Usage: " << argv[0] << "<filename> x y z\n";
      return 1;
  }
  
  // use scoped lifetimes of wrappers to release everything before ospShutdown()
  {
    
    GLFWOSPWindow glfwOspWindow;

    // create and setup model and mesh
    
    vec3i volumeDimensions(656, 256, 200);
    int numPoints{10};
    //std::vector<std::vector<float> > voxels = generateVoxels_3ch(volumeDimensions, numPoints);
    //std::vector<std::vector<float> > voxels = generateVoxels_nch(volumeDimensions, numPoints, 1);
    //std::cout << voxels.size()<<" channels "
    //	      << volumeDimensions.x <<"x"
    //	      << volumeDimensions.y <<"x"
    //	      << volumeDimensions.z <<" "
    //	      << voxels[0].size()<< " total voxels" << std::endl;
    
    std::vector<ospray::cpp::SharedData> voxel_data;
    std::vector<ospray::cpp::TransferFunction> tfns;
    std::vector<vec3f> colors = {vec3f(1,0,0), vec3f(0,1,0),vec3f(0,0,1)};
    uint32_t n_of_ch = 3;
    /*for (const auto &v : voxels) {
      voxel_data.push_back(ospray::cpp::SharedData(v.data(), volumeDimensions));
      }*/
    
    std::fstream file;
    file.open(argv[1], std::fstream::in | std::fstream::binary);
    std::cout <<"dim "<<argv[2]<<" "<<argv[3]<<" "<<argv[4]<<"\n";
    std::vector<std::vector<float> > voxels_read;
    voxels_read.resize(n_of_ch);

    for (uint32_t j =0; j<n_of_ch; j++){
      float min=math::inf, max=0;
      for(int i=0; i<volumeDimensions.long_product();i++){
	uint16_t buff;
	file.read((char*)(&buff), sizeof(buff));
	voxels_read[j].push_back(float(buff));
      
	//voxels_read[1].push_back(float(buff));
	if (float(buff) > max) max = float(buff);
	if (float(buff) < min) min = float(buff);
      }
      tfns.push_back(makeTransferFunctionForColor(vec2f(min, 1000), colors[j]));

    }
    file.close();
    
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
    model.setParam("transferFunction", tfns[0]);
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
    }


    // complete setup of renderer
    renderer->setParam("aoSamples", 1);
    renderer->setParam("backgroundColor", 0.f); // white, transparent
    renderer->setParam("blendMode", glfwOspWindow.blendMode); // 0:add color 1: alpha blend
    renderer->setParam("renderAttributes", ospray::cpp::CopiedData(glfwOspWindow.renderAttributesData));
    renderer->setParam("numAttributes", voxels_read.size());
    renderer->setParam("tfnType", glfwOspWindow.tfnType); // 0:same tfn all channel 1: pick evenly on hue
    renderer->setParam("transferFunctions", ospray::cpp::CopiedData(tfns)); 
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

    // initialize GLFW
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW!");
    }

    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    // create GLFW window
    glfwWindow = glfwCreateWindow(windowSize.x, windowSize.y, "OSPRay Tutorial", nullptr, nullptr);

    if (!glfwWindow) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window!");
    }

    // make the window's context current
    glfwMakeContextCurrent(glfwWindow);

    ImGui_ImplGlfwGL3_Init(glfwWindow, true);
    ImGui::StyleColorsDark();
    
    auto fb = glfwOspWindow.framebuffer.map(OSP_FB_COLOR);
    init(fb);
    glfwOspWindow.framebuffer.unmap(fb);
    glfwOspWindow.setFunc();
    glfwOspWindow.reshape(windowSize.x, windowSize.y);

    glfwSetInputMode(glfwWindow, GLFW_STICKY_KEYS, GL_TRUE);

    do{
      
      glClear(GL_COLOR_BUFFER_BIT);
      glEnable(GL_TEXTURE_2D);
      
      ImGui_ImplGlfwGL3_NewFrame();
      glfwOspWindow.buildUI();

      
      glEnable(GL_FRAMEBUFFER_SRGB); // Turn on sRGB conversion for OSPRay frame
      glfwOspWindow.display();
      glDisable(GL_FRAMEBUFFER_SRGB); // Disable SRGB conversion for UI
      glDisable(GL_TEXTURE_2D);
      
      ImGui::Render();
      ImGui_ImplGlfwGL3_Render();
      
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



