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

//#include <vector>

#include "ospray/ospray_cpp.h"
#include "ospray/ospray_cpp/ext/rkcommon.h"
#include "rkcommon/utility/SaveImage.h"
#include "ArcballCamera.h"

#include "voxelGeneration.h"

// stl
#include <random>
#include <vector>


#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>



using namespace rkcommon;
using namespace rkcommon::math;


// image size
vec2i imgSize{1024, 768};
vec2i windowSize{1024,768};
unsigned int texture;


class GLUTOSP_OBJECTS{
public:
  ospray::cpp::Camera camera{"perspective"};
  ospray::cpp::Renderer renderer{"multivariant"};
  ospray::cpp::World world;

  bool leftDown = false;
  bool rightDown = false;
  bool middleDown = false;
  vec2f previousMouse{vec2f(-1)};
  
  static GLUTOSP_OBJECTS *activeObj;
  ospray::cpp::FrameBuffer framebuffer;
  std::unique_ptr<ArcballCamera> arcballCamera;

  
  GLUTOSP_OBJECTS(){
    activeObj = this;
    
    /// prepare framebuffer
    auto buffers = OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM | OSP_FB_ALBEDO
      | OSP_FB_NORMAL;
    framebuffer = ospray::cpp::FrameBuffer(imgSize.x, imgSize.y, OSP_FB_RGBA32F, buffers);
    
  }
  void display();
  void motion(int, int);
  void mouse(int, int, int, int);
  
  void setFunc(){
    glutMouseFunc([](int button, int state, int x, int y){
		    activeObj->mouse(button, state, x, y);
		  });

    glutMotionFunc([](int x, int y){
		    activeObj->motion(x, y);
		  });
    
    glutDisplayFunc([](){
		      activeObj->display();
		  });

    
  }

  void renderNewFrame(){
    framebuffer.clear();
    // render one frame
    framebuffer.renderFrame(renderer, camera, world);
  }
};

GLUTOSP_OBJECTS *GLUTOSP_OBJECTS::activeObj = nullptr;



void GLUTOSP_OBJECTS::display()
{ 
  
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glEnable(GL_TEXTURE_2D);
   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
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
   //glFlush();
   
   glutSwapBuffers();
   glDisable(GL_TEXTURE_2D);
   glutPostRedisplay();
}


void GLUTOSP_OBJECTS::mouse(int button, int state,int x, int y){
  leftDown = (button == GLUT_LEFT_BUTTON);
  rightDown = (button == GLUT_RIGHT_BUTTON);
  middleDown = (button == GLUT_MIDDLE_BUTTON);

  if (state == GLUT_UP)
    previousMouse = vec2f(-1);
}

void GLUTOSP_OBJECTS::motion(int x, int y){
  const vec2f mouse(x, y);
  if (previousMouse != vec2f(-1)) {
    const vec2f prev = previousMouse;

    bool cameraChanged = leftDown || rightDown || middleDown;
    //std::cout << leftDown << "   mouse left \n";

    if (leftDown) {
      const vec2f mouseFrom(clamp(prev.x * 2.f / windowSize.x - 1.f, -1.f, 1.f),
          clamp(prev.y * 2.f / windowSize.y - 1.f, -1.f, 1.f));
      const vec2f mouseTo(clamp(mouse.x * 2.f / windowSize.x - 1.f, -1.f, 1.f),
          clamp(mouse.y * 2.f / windowSize.y - 1.f, -1.f, 1.f));
      arcballCamera->rotate(mouseFrom, mouseTo);
      //std::cout << "r" << mouseFrom.x << " " <<mouseFrom.y << " "<< mouseTo.x <<" "<<mouseTo.y <<"\n";
    } else if (rightDown) {
      arcballCamera->zoom(mouse.y - prev.y);
    } else if (middleDown) {
      arcballCamera->pan(vec2f(mouse.x - prev.x, prev.y - mouse.y));
    }

    if (cameraChanged) {
      //updateCamera();
      camera.setParam("aspect", windowSize.x / float(windowSize.y));
      camera.setParam("position", arcballCamera->eyePos());
      camera.setParam("direction", arcballCamera->lookDir());
      camera.setParam("up", arcballCamera->upDir());
      camera.commit();
    }
  }

  previousMouse = mouse;

}



void reshape(int w, int h)
{
  windowSize.x = w;
  windowSize.y = h;
  glViewport(0, 0, windowSize.x, windowSize.y);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, windowSize.x, 0.0, windowSize.y, -1.0, 1.0);
}

void init (void* fb){
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
  
  // use scoped lifetimes of wrappers to release everything before ospShutdown()
  {
    
    GLUTOSP_OBJECTS glutOsp_objs;

    // create and setup model and mesh
    vec3i volumeDimensions{128};
    int numPoints{10};
    std::vector<std::vector<float> > voxels = generateVoxels_3ch(volumeDimensions, numPoints);
    std::cout << voxels.size()<<" channels "
	      << volumeDimensions.x <<"x"
	      << volumeDimensions.y <<"x"
	      << volumeDimensions.z <<" "
	      << voxels[0].size()<< " total voxels" << std::endl;
    
    std::vector<ospray::cpp::SharedData> voxel_data;
    for (const auto &v : voxels) {
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
    model.setParam("transferFunction", makeTransferFunction(vec2f(0.f, 10.f)));
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
    glutOsp_objs.world.setParam("instance", ospray::cpp::CopiedData(instance));

    // create and setup light for Ambient Occlusion
    ospray::cpp::Light light("ambient");
    light.commit();

    glutOsp_objs.world.setParam("light", ospray::cpp::CopiedData(light));
    glutOsp_objs.world.commit();

    // create renderer, choose Scientific Visualization renderer
    // load multivariant renderer module
    ospray::cpp::Renderer *renderer = &glutOsp_objs.renderer;
    //ospray::cpp::Renderer renderer("scivis");
    std::cout << "using multivariant renderer \n";

    
    // complete setup of renderer
    renderer->setParam("aoSamples", 1);
    renderer->setParam("backgroundColor", 1.0f); // white, transparent
    renderer->commit();

    // create and setup camera
    
    // set up arcball camera for ospray
    glutOsp_objs.arcballCamera.reset(new ArcballCamera(glutOsp_objs.world.getBounds<box3f>(), windowSize));
    glutOsp_objs.arcballCamera->eyePos() = cam_pos;
    glutOsp_objs.arcballCamera->lookDir() = cam_view;
    glutOsp_objs.arcballCamera->upDir() = cam_up;
    
    glutOsp_objs.arcballCamera->updateWindowSize(windowSize);
    
    ospray::cpp::Camera* camera = &glutOsp_objs.camera;
    camera->setParam("aspect", imgSize.x / (float)imgSize.y);
    camera->setParam("position", glutOsp_objs.arcballCamera->eyePos());
    camera->setParam("direction", glutOsp_objs.arcballCamera->lookDir());
    camera->setParam("up", glutOsp_objs.arcballCamera->upDir());
    camera->commit(); // commit each object to indicate modifications are done

    
    glutOsp_objs.renderNewFrame();

    
    glutInit(&argc, (char**)argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowSize.x, windowSize.y);
    glutInitWindowPosition(100, 100);
    glutCreateWindow(argv[0]);
       
    auto fb = glutOsp_objs.framebuffer.map(OSP_FB_COLOR);
    init(fb);
    glutOsp_objs.framebuffer.unmap(fb);
 
    glutReshapeFunc(reshape);
    glutOsp_objs.setFunc();
    glutMainLoop();

   
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



