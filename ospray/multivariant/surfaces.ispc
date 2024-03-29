// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "math/random.ih"
#include "math/sampling.ih"
#include "render/util.ih"
// Multivariant renderer
#include "MultivariantMaterial.ih"
#include "lights/Light.ih"
#include "surfaces.ih"

vec3f directIllumination(const uniform Multivariant *uniform self,
    const FrameBuffer *uniform fb,
    const World *uniform world,
    const varying DifferentialGeometry &dg,
    ScreenSample &sample,
    varying LDSampler *uniform ldSampler,
    const varying MultivariantBSDF &bsdf,
    const varying vec3f &inDir,
    const float time)
{
  vec3f color = make_vec3f(0.f);
  if (!world->scivisData.lights)
    return color;

  // calculate shading for all lights
  for (uniform int i = 0; i < world->scivisData.numLights; i++) {
    const uniform Light *uniform l = world->scivisData.lights[i];
    const vec2f s = make_vec2f(0.0f); // sample center of area lights
    const Light_SampleRes light = l->sample(l, dg, s);

    if (reduce_max(light.weight) > 0.f) { // any potential contribution?
      const float cosNL = dot(light.dir, dg.Ns);
      if (cosNL < 0.0f)
        continue;

      const vec3f H = normalize(light.dir - inDir);
      const float cosNH = dot(H, dg.Ns);
      const vec3f specular = bsdf.specular * powf(cosNH, bsdf.shininess);
      const vec3f diffuse = bsdf.diffuse * cosNL;
      const vec3f light_contrib = (diffuse + specular) * light.weight;

      if (self->shadowsEnabled) {
        if (luminance(light_contrib) > self->super.minContribution) {
          vec3f P = dg.P;
          if (dot(dg.Ng, light.dir) < 0.0f)
            P = P - (2.f * dg.epsilon) * dg.Ng;

          Ray shadowRay;
          setRay(shadowRay, P, light.dir, 0.0f, light.dist, time);

          vec3f light_alpha = lightAlpha(self,
              shadowRay,
              fb,
              world,
              sample,
              ldSampler,
              light_contrib,
              dg.epsilon,
              0.25f);

          color = color + light_alpha * light_contrib;
        }
      } else {
        color = color + light_contrib;
      }
    }
  }

  return color;
}

SSI computeShading(const Multivariant *uniform self,
    const FrameBuffer *uniform fb,
    const World *uniform world,
    const DifferentialGeometry &dg,
    ScreenSample &sample,
    varying LDSampler *uniform ldSampler,
    const varying vec3f &inDir,
    const float time)
{
  SSI retval;

  const MultivariantBSDF bsdf = evalMaterial(dg);
  retval.albedo = bsdf.albedo;

  vec3f color = directIllumination(
      self, fb, world, dg, sample, ldSampler, bsdf, inDir, time);

  vec3f ao = make_vec3f(1.f);
  if (self->aoSamples > 0
      && luminance(world->scivisData.aoColorPi) > self->super.minContribution)
    ao = Multivariant_computeAO(self,
        fb,
        world,
        sample,
        ldSampler,
        dg,
        self->aoSamples,
        self->aoRadius,
        sample.sampleID);

  color = color + bsdf.diffuse * ao * world->scivisData.aoColorPi;

  retval.shadedColor = color;
  retval.transmission = bsdf.transmission;

  return retval;
}

vec3f evaluateLights(
    const World *uniform world, const DifferentialGeometry &dg, const Ray &ray)
{
  // Exit if no lights
  vec3f color = make_vec3f(0.f);
  if (!world->scivisData.lightsVisible)
    return color;

  // Iterate through all lights
  for (uniform int i = 0; i < world->scivisData.numLightsVisible; i++) {
    // Skip lights with disabled visibilty via parameter
    const uniform Light *uniform l = world->scivisData.lightsVisible[i];
    if (!l->isVisible)
      continue;

    // Evaluate light contribution
    const Light_EvalRes le = l->eval(l, dg, ray.dir, ray.t0, ray.t);
    color = color + le.radiance;
  }
  return color;
}
