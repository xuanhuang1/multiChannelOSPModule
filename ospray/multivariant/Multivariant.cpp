// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "Multivariant.h"
#include "lights/AmbientLight.h"
#include "lights/HDRILight.h"
#include "lights/SunSkyLight.h"
// ispc exports
#include "common/World_ispc.h"
#include "multivariant/Multivariant_ispc.h"

namespace ospray {

Multivariant::Multivariant()
{
  ispcEquivalent = ispc::Multivariant_create(this);
}

std::string Multivariant::toString() const
{
  return "ospray::render::Multivariant";
}

void Multivariant::commit()
{
  Renderer::commit();

  visibleLights = getParam<bool>("visibleLights", false);

  renderAttributes = getParamDataT<int>("renderAttributes", false);
  renderAttributesWeights = getParamDataT<float>("renderAttributesWeights", false);
  histMaskTexture = getParamDataT<unsigned char>("histMaskTexture", false);
  bbox = getParamDataT<float>("bbox", false);
  
  tfs = getParamDataT<TransferFunction *>("transferFunctions", false);
  
  auto *distFunction = (TransferFunction *)getParamObject("distanceFunction", nullptr);

  if (distFunction == nullptr)
    throw std::runtime_error("volumetric model must have 'distanceFunction'");

  
  tfIEs = createArrayOfIE(*tfs);
  
  ispc::Multivariant_set(getIE(),
			 getParam<bool>("shadows", false),
			 getParam<int>("aoSamples", 0),
			 getParam<float>("aoDistance", getParam<float>("aoRadius", 1e20f)),
			 getParam<float>("volumeSamplingRate", 1.f),
			 getParam<int>("blendMode", 0),
			 getParam<int>("frontBackBlendMode", 0),
			 getParam<int>("shadeMode", 0),
			 ispc(bbox),
			 ispc(renderAttributes),
			 ispc(renderAttributesWeights),
			 ispc(histMaskTexture),
			 getParam<int>("numAttributes", 0),
			 getParam<int>("tfnType", 0),
			 tfIEs.data(),
			 distFunction->getIE()
			 );
}


  // WORLD SCIVISDATA?
void *Multivariant::beginFrame(FrameBuffer *, World *world)
{
  if (!world)
    return nullptr;

  const bool visibleLightListValid = visibleLights == scannedVisibleLightList;

  if (world->scivisDataValid && visibleLightListValid)
    return nullptr;

  std::vector<void *> lightArray;
  std::vector<void *> lightVisibleArray;
  vec3f aoColor = vec3f(0.f);

  if (world->lights) {
    for (auto &&light : *world->lights) {
      // extract color from ambient lights and remove them
      const auto ambient = dynamic_cast<const AmbientLight *>(light);
      if (ambient) {
        aoColor += ambient->radiance;
        if (visibleLights)
          lightVisibleArray.push_back(light->getIE());
        continue;
      }

      // hdri lights are only (potentially) visible, no illumination
      const auto hdri = dynamic_cast<const HDRILight *>(light);
      if (hdri) {
        if (visibleLights)
          lightVisibleArray.push_back(light->getIE());
        continue;
      }

      // sun-sky: only sun illuminates, sky only (potentially) in background
      const auto sunsky = dynamic_cast<const SunSkyLight *>(light);
      if (sunsky) {
        lightArray.push_back(light->getSecondIE().value()); // sun
        if (visibleLights) {
          lightVisibleArray.push_back(light->getIE()); // sky
          lightVisibleArray.push_back(light->getSecondIE().value()); // sun
        }
        continue;
      }

      // handle the remaining types of lights
      lightArray.push_back(light->getIE());
      if (visibleLights)
        lightVisibleArray.push_back(light->getIE());
    }
  }

  ispc::World_setSciVisData(world->getIE(),
      (ispc::vec3f &)aoColor,
      lightArray.empty() ? nullptr : &lightArray[0],
      lightArray.size(),
      lightVisibleArray.empty() ? nullptr : &lightVisibleArray[0],
      lightVisibleArray.size());

  world->scivisDataValid = true;
  scannedVisibleLightList = visibleLights;

  return nullptr;
}

} // namespace ospray
