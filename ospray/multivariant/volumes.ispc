// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "surfaces.ih"
#include "volumes.ih"
// ispc device
#include "math/random.ih"
#include "math/sampling.ih"
#include "render/util.ih"

#include "openvkl/openvkl.isph"

struct VolumeContext
{
  uniform unsigned int8 intervalIteratorBuffer[VKL_MAX_INTERVAL_ITERATOR_SIZE];
  VKLIntervalIterator intervalIterator;
  VKLInterval interval;
  vec3f org;
  vec3f dir;
  float iuLength; // VKL interval length in sampling steps as an unit
  float iuDistance; // sampling distance within VKL interval in sampling steps
                    // as an unit
  float distance; // last sampling distance from 'vc.org'
  vec4f sample;
  uint32 ready; // 1 if sample is ready to be used
};


struct vec4f addBlend(struct vec4f a, struct vec4f b)
{
  struct vec4f ret = {0.f, 0.f, 0.f, 0.f};
  ret.w = clamp(a.w + b.w, 0.f, 1.f);
  if (ret.w == 0.f) return ret;
  
  ret.x = clamp((a.x*a.w + b.x*b.w)/ret.w, 0.f, 1.f);
  ret.y = clamp((a.y*a.w + b.y*b.w)/ret.w, 0.f, 1.f);
  ret.z = clamp((a.z*a.w + b.z*b.w)/ret.w, 0.f, 1.f);
  return ret;
}


struct vec4f alphaBlend(struct vec4f a, struct vec4f b)
{
  struct vec4f ret = {0.f, 0.f, 0.f, 0.f};
  ret.w = clamp(a.w + b.w*(1-a.w), 0.f, 1.f);
  if (ret.w == 0.f) return ret;
  
  ret.x = clamp((a.x*a.w + b.x*b.w*(1-a.w))/ret.w, 0.f, 1.f);
  ret.y = clamp((a.y*a.w + b.y*b.w*(1-a.w))/ret.w, 0.f, 1.f);
  ret.z = clamp((a.z*a.w + b.z*b.w*(1-a.w))/ret.w, 0.f, 1.f);
  return ret;
}


struct vec4f porterDuff(struct vec4f c1, struct vec4f c2, float a1,  float a2)
{
  // will disgard .w component
  
  struct vec4f ret = {0.f, 0.f, 0.f, 0.f};
  ret.w = clamp(a1 + a2, 0.f, 1.f);	
  if (ret.w == 0.f) return ret;
  
  ret.x = clamp((c1.x*a1 + c2.x*a2)/ret.w, 0.f, 1.f);
  ret.y = clamp((c1.y*a1 + c2.y*a2)/ret.w, 0.f, 1.f);
  ret.z = clamp((c1.z*a1 + c2.z*a2)/ret.w, 0.f, 1.f);
  return ret;
}

struct vec4f rgb2hsl(struct vec4f c){
       // keep w alpha component
       // range (0, 1) to (0 ,1)
       float H = 0.f; float S = 0.f; float L = 0.f;
       float R = c.x; float G = c.y; float B = c.z;

       float Vmax = max(max(R, G), B);
       float Vmin = min(min(R, G), B);

       L = (Vmax + Vmin)/2.f;
       float delta = (Vmax - Vmin);

       if (Vmax == Vmin) H = 0.f;
       else if (Vmax == R) H = 60*(G-B)/(Vmax - Vmin);
       else if (Vmax == G) H = 120 + 60*(B-R)/(Vmax - Vmin);
       else if (Vmax == B) H = 240 + 60*(R-G)/(Vmax - Vmin);

       if (H<0) H = H + 360;

       if (delta == 0) S = 0.f;
       else S = (Vmax - Vmin)/(1 - abs(2*L - 1));

       struct vec4f ret = {H, S, L, c.w};
       ret.x = clamp(ret.x, 0.f, 360.f);
       ret.y = clamp(ret.y, 0.f, 1.f);
       ret.z = clamp(ret.z, 0.f, 1.f);
       return ret; 
}

struct vec4f hsl2rgb(struct vec4f c){
       float H = c.x;	  float S = c.y; float L=c.z;
       float R = 0;   	  float G = 0;   float B = 0;
       
       float C = (1 - abs(2*L - 1))*S;
       float tmpHMod = ( (int)((H/60)*100) %200)/100.f;
       float X = C*(1 - abs(tmpHMod-1));
       float m = L - C/2;

       if (H < 60)	{ R = C; G = X; B = 0;}
       else if( H<120 )	{ R = X; G = C; B = 0;}
       else if( H<180 )	{ R = 0; G = C; B = X;}
       else if( H<240 )	{ R = 0; G = X; B = C;}
       else if( H<300 )	{ R = X; G = 0; B = C;}
       else 		{ R = C; G = 0; B = X;}

       struct vec4f ret = {R+m, G+m, B+m, c.w};
       ret.x = clamp(ret.x, 0.f, 1.f);
       ret.y = clamp(ret.y, 0.f, 1.f);
       ret.z = clamp(ret.z, 0.f, 1.f);
       return ret;
}

struct vec4f huePreserveBlend(struct vec4f c1, struct vec4f c2){
       struct vec4f c_new = c1;
       struct vec4f c1_hsl = rgb2hsl(c1);
       struct vec4f c2_hsl = rgb2hsl(c2);
       
       struct vec4f c_new_hsl;

       // get result of normal blending
       struct vec4f c_add_hsl = porterDuff(c1, c2, c1.w, c2.w);
       c_add_hsl = rgb2hsl(c_add_hsl);

       // assign L
       c_new_hsl.z = min(c_add_hsl.z, 1.f);

       // get weights from alpha to perform lerp on satuation
       float a1 = c1.w / (c1.w + c2.w);
       float a2 = c2.w / (c1.w + c2.w);

       if (abs(c1_hsl.x - c2_hsl.x) < 0.001)
       	  return porterDuff(c1, c2, c1.w, c2.w);
       else{
	 if (c1_hsl.y*a1 > c2_hsl.y*a2){
	     c_new_hsl.x = c1_hsl.x;
	     c_new_hsl.y = min(c1_hsl.y*a1 - c2_hsl.y*a2, 1.f);
	 }else{
	     c_new_hsl.x = c2_hsl.x;
	     c_new_hsl.y = min(c2_hsl.y*a2 - c1_hsl.y*a1, 1.f);
	 }
       }
       
       c_new = hsl2rgb(c_new_hsl);
       c_new.w = min(c1.w + c2.w, 1.f);
       //c_new.x = clamp(c_new.x/c_new.w, 0.f, 1.f);
       //c_new.y = clamp(c_new.y/c_new.w, 0.f, 1.f);
       //c_new.z = clamp(c_new.z/c_new.w, 0.f, 1.f);

       return c_new;
}

struct vec4f highestDominateBlend(struct vec4f c1, struct vec4f c2, float val_a, float val_b){
       if (val_a > val_b) return c1;
       else return c2;
}


struct vec4f blendByMode(struct vec4f a, struct vec4f b, float val_a, float val_b, uniform unsigned int blendMode, const uniform Multivariant *uniform self){

  struct vec4f ret = {1.f, 1.f, 1.f, 1.f};
  
  if (blendMode == 0)
     ret = addBlend(a, b);
  else if (blendMode == 1)
     ret = alphaBlend(a, b);
  else if (blendMode == 2)
     ret = huePreserveBlend(a, b);
  else if (blendMode == 3)
     ret = highestDominateBlend(a, b, a.w, b.w);
  else if (blendMode == 4)
     ret = highestDominateBlend(a, b, a.w, b.w);
  return ret;
  
}


struct vec4f blendWithSameTF(varying float* samples,
	       	     	     uniform unsigned int32 M,
			     VolumetricModel *uniform m,
			     uniform unsigned int blendMode,
    			     const uniform Multivariant *uniform self,
			     uniform unsigned int *uniform attributeIndices)
{
	
  struct vec4f ret = {0.f, 0.f, 0.f, 1.f};
  
      
  for (uniform i=0; i<M; i++){
      uniform int tfn_index = 0;//attributeIndices[i];
      struct vec4f tmp = self->tfns[tfn_index]->get(self->tfns[tfn_index], samples[i]);
      //struct vec4f tmp = {0.f, 1.f, 1.f, 1.f};
      if (i == 0) ret = tmp;
      else ret = blendByMode(ret, tmp, ret.w, tmp.w,  blendMode, self);
  }
  
  return ret;
}

static void quick_sort (varying unsigned int32 *varying a , varying  unsigned int32 n,  varying unsigned int32* varying order)
{
  uint p, t;
  uint i, j;

  if (n < 2) return;

  p = order[n/2];

  for (i = 0, j = n - 1;; i++, j--)
  {
    while (order[i] < p) i++;
    while (p < order[j]) j--;
    if (i >= j) break;

    t = a[i];
    a[i] = a[j];
    a[j] = t;

    t = order[i];
    order[i] = order[j];
    order[j] = t;
  }

  quick_sort (a, i, order);
  quick_sort (a+i, n-i, order+i);
}

struct vec4f blendWithDiffHue(varying float* samples,
       	     		      uniform unsigned int32 M,
			      VolumetricModel *uniform m,
			      uniform unsigned int blendMode,
    			      const uniform Multivariant *uniform self,
			      uniform unsigned int *uniform attributeIndices)
{
  // pick color evenly between RGB hue
  struct vec4f ret = {0.f, 0.f, 0.f, 0.f};

  struct vec4f c_maxs1 = {0.f, 0.f, 0.f, 0.f};
  struct vec4f c_maxs2 = {0.f, 0.f, 0.f, 0.f};
				 
  float sample = 0;

  // if doing alphaBlend
  // shuffle  channel ordering within a voxel
  unsigned int order[128];
  unsigned int attributeIndicesRand[128];
  struct vec4f base_colors[128];
  vkl_range1f ranges[128];

  RNGState state;
  seed_rng(&state, programIndex);
  
  for (uniform int i=0; i<M; i++){
      uniform uint tfn_index = attributeIndices[i];
      order[i] = random(&state)%3;
      attributeIndicesRand[i] = attributeIndices[(i)%M];
      base_colors[i] = self->tfns[tfn_index]->get(self->tfns[tfn_index], samples[i]);
      ranges[i]= vklGetValueRange(m->volume->vklVolume, i);
  }
 
  //quick_sort(attributeIndicesRand, M, order);

  //if (attributeIndicesRand[0] == attributeIndices[0]) ret.x = 1.f;
  //if (attributeIndicesRand[0] == attributeIndices[1]) ret.y = 1.f;
  //if (attributeIndicesRand[0] == attributeIndices[2]) ret.z = 1.f;
  //ret.w = 1.f;
  //return ret;
  //if (order[0] == 0) ret.x = 1.f;
  //ret.w = 1.f;
  //return ret;

  int prev_i = 0;
  
  for (uniform int i=0; i<M; i++){
      struct vec4f base_color = base_colors[i];
      if (blendMode == 0)
      	 ret = addBlend(ret, base_color);
      else if (blendMode == 1)
     	  ret = alphaBlend(ret, base_color);
      else if (blendMode == 2){ 
	struct vec4f c_hsl = base_color;
	if (i==0) {
	   //c_maxs2 = c_hsl;
	   c_maxs1 = c_hsl;
	}else if (i==1) {
	   if (c_maxs1.w < c_hsl.w){
	      c_maxs2 = c_maxs1;
	      c_maxs1 = c_hsl;
	   }else{
	      c_maxs2 = c_hsl;
	   }
	}
	else if (c_maxs2.w < c_hsl.w ){
	   if (c_maxs1.w < c_hsl.w){
	      c_maxs2 = c_maxs1;
	      c_maxs1 = c_hsl;
	   }else{
	      c_maxs2 = c_hsl;	
	   }
	}
      }else if (blendMode == 3){
     	  ret = highestDominateBlend(ret, base_color, ret.w, base_color.w);
      }else if (blendMode == 4){
      	  if (i == 0) prev_i = 0;
	  float prev_weight = get_float(self->renderAttributesWeights, prev_i);
	  float this_weight = get_float(self->renderAttributesWeights, i);
	  
	  float prev_val = (samples[prev_i] - ranges[prev_i].lower) / (ranges[prev_i].upper - ranges[prev_i].lower);
	  float this_val = (samples[i] - ranges[i].lower) / (ranges[i].upper - ranges[i].lower);

	  ret = highestDominateBlend(ret, base_color, prev_weight*prev_val, this_weight*this_val);
	  
	  if (prev_weight*prev_val <= this_weight*this_val)
	     prev_i = i;
      }else if (blendMode == 5){
      	    //
	    //
	    //		LOTS OF HARDCODED PARAMETERS
	    //
	    //
	    //
      	  if (i ==1){
	  //float prev_weight = get_float(self->renderAttributesWeights, prev_i);
	  //float this_weight = get_float(self->renderAttributesWeights, i);
	  
	  int prev_val = (samples[prev_i] - ranges[prev_i].lower) / (ranges[prev_i].upper - ranges[prev_i].lower) * 100.f;
	  int this_val = (samples[i] - ranges[i].lower) / (ranges[i].upper - ranges[i].lower)* 100.f;

	  // modify here to sample texture for rgb value
	  // use only alpha from the first channel
	  // 2d index is [prev_val][this_val] col major texture image
	  int idx = prev_val*100*4 + this_val*4;

	  float r = get_uint8(self->histMaskTexture, idx) / 255.f;
	  float g = get_uint8(self->histMaskTexture, idx+1) / 255.f;
	  float b = get_uint8(self->histMaskTexture, idx+2) / 255.f;
	  ret.x = r; ret.y = g; ret.z = b; ret.w = base_colors[0].w;

	  }
      }
  }

  if (blendMode == 2){
     ret = huePreserveBlend(c_maxs1, c_maxs2);
  }

  // test color conversion
  //ret = rgb2hsl(ret);
  //ret = hsl2rgb(ret);
  return ret;
}



static void sampleVolume(MultivariantRenderContext &rc,
    VolumeContext &vc,
    VolumetricModel *uniform m,
    Ray &ray,
    const VolumeInterval &vi,
    const uniform float samplingRate,
    const uniform bool shade,
    const uniform Multivariant *uniform self)
{
  // We have to iterate till we get a valid sample value
  float dt;
  float sampleVal = nan;
  const uniform float gsc = shade ? m->gradientShadingScale : 0.f;
  vec3f p; // in volume local coords

  // Sample multi channel volume value in given point
  uniform unsigned int M = self->renderAttributes.numItems;  //m->volume->vklVolume.numAttributes;
  uniform unsigned int blendMode = self->blendMode;

  // set a max of 128
  uniform unsigned int attributeIndices[128];
  float samples[128];
  
  for (uniform int i=0; i<M; i++){
      attributeIndices[i] = get_int32(self->renderAttributes, i);
  }

  while (isnan(sampleVal)) {
    // Iterate till sampling position is within interval
    float emptySpace = 0.f;
    while (vc.iuDistance > vc.iuLength) {
      // Get next VKL interval
      const float prevUpper = vc.interval.tRange.upper;
      if (vklIterateIntervalV(vc.intervalIterator, &vc.interval)) {
        // Intervals may not be contiguous, accumulate empty space
        emptySpace += max(vc.interval.tRange.lower - prevUpper, 0.f);

        // Make it local for the next interval
        vc.iuDistance -= vc.iuLength;

        // Calulate how many steps can be made within this interval
        const float samplingStep = vc.interval.nominalDeltaT / samplingRate;
        vc.iuLength = (vc.interval.tRange.upper - vc.interval.tRange.lower)
            / samplingStep;

        // Initialize distance if necessary
        vc.distance =
            (vc.distance == inf) ? vc.interval.tRange.lower : vc.distance;

      } else {
        // The end of the volume has been reached
        vc.distance = inf;
        return;
      }
    }

    // Calculate sampling distance
    const float samplingStep = vc.interval.nominalDeltaT / samplingRate;
    const float newDistance =
        vc.interval.tRange.lower + vc.iuDistance * samplingStep;

    // Prepare sampling position
    p = vc.org + newDistance * vc.dir;


    vklComputeSampleMV(
        m->volume->vklSampler, (const varying vkl_vec3f *uniform) & p,
	samples, M, attributeIndices);
    sampleVal = samples[0];
	
    // Go to the next sub-interval
    vc.iuDistance += 1.f;
    dt = newDistance - vc.distance - emptySpace;
    vc.distance = newDistance;
  }

  // Apply transfer function to get color with alpha
  if (M == 1)
     vc.sample = self->tfns[attributeIndices[0]]->get(self->tfns[attributeIndices[0]], sampleVal);
  else
  {
     if (self->tfnType == 0) 
     	vc.sample = blendWithSameTF(samples, M, m, blendMode, self, attributeIndices);
     else //if (self->tfnType == 1)
        vc.sample = blendWithDiffHue(samples, M, m, blendMode, self, attributeIndices);
  }

  // compute gradient shading lighting
  if (gsc > 0.0f) {
    vec3f ns = Volume_getGradient(m->volume, p);
    if (dot(ns, ns) > 1e-6f) {
      // assume that opacity directly correlates to volume scalar field, i.e.
      // that "outside" has lower values; because the gradient point towards
      // increasing values we need to flip it
      ns = neg(ns);

      // use scivis shading function
      // Prepare differential geometry structure
      DifferentialGeometry dg;
      dg.color = vc.sample;
      dg.material = NULL;
      dg.epsilon = dt / 2.f;
      // transform to world coords
      dg.Ns = dg.Ng = normalize(xfmVector(transposed(vi.instance->xfm.l), ns));
      dg.P = ray.org + vc.distance * ray.dir;
      SSI shading = computeShading(rc.renderer,
          rc.fb,
          rc.world,
          dg,
          rc.sample,
          rc.ldSampler,
          ray.dir,
          0.f);
      vec4f shadedColor = make_vec4f(
          shading.shadedColor, 1.f - luminance(shading.transmission));
      vc.sample = lerp(gsc, vc.sample, shadedColor);
    }
  }

  // Weight the opacity with deltaT using Beer law
  vc.sample.w = exp(-vc.sample.w * dt * m->densityScale);
}

static float sampleAllVolumes(MultivariantRenderContext &rc,
    const VolumeIntervals &volumeIntervals,
    varying VolumeContext *uniform volumeContexts,
    Ray &ray,
    const uniform float samplingRate,
    vec4f &sampledColor,
    const uniform bool shade,
    const uniform Multivariant *uniform self)
{
  // Look for the closest sample across all volumes
  float minDist = inf;
  int usedSampleId = -1;
  for (uniform int i = 0; i < reduce_max(volumeIntervals.numVolumeIntervals);
       i++) {
    if (i >= volumeIntervals.numVolumeIntervals)
      break;

    // If the sample has been used already regenerate a new one
    VolumeContext &vc = volumeContexts[i];
    if (vc.ready == 0) {
      const VolumeInterval &vi = volumeIntervals.intervals[i];
      foreach_unique (m in vi.volumetricModel) {
        sampleVolume(rc, vc, m, ray, vi, samplingRate, shade, self);
      }
      vc.ready = 1;
    }

    // Take the sample if closer
    if (vc.distance < minDist) {
      minDist = vc.distance;
      sampledColor = vc.sample;
      usedSampleId = i;
    }
  }

  // Mark used samples as not ready
  if (usedSampleId != -1) {
    volumeContexts[usedSampleId].ready = 0;
  }

  // Return distance for sampled color
  return minDist;
}

vec4f integrateVolumeIntervalsGradient(MultivariantRenderContext &rc,
    const VolumeIntervals &volumeIntervals,
    const RayIntervals &rayIntervals,
    Ray &ray,
    varying LDSampler *uniform ldSampler,
    const uniform float samplingRate,
    const uniform bool shade,
    const uniform Multivariant *uniform self)
{
  // Array of volume contexts
  varying VolumeContext *uniform volumeContexts =
      (varying VolumeContext * uniform)
          pushTLS(reduce_max(volumeIntervals.numVolumeIntervals)
              * sizeof(varying VolumeContext));

  // Sampling position jitter
  const float jitter = LDSampler_getFloat(ldSampler, 0);

  // Iterate through all volumes and initialize its contexts with data that
  // do not change across ray intervals
  for (uniform int i = 0; i < reduce_max(volumeIntervals.numVolumeIntervals);
       i++) {
    if (i >= volumeIntervals.numVolumeIntervals)
      break;

    // Transform ray into the volume local space
    const VolumeInterval &vi = volumeIntervals.intervals[i];
    Ray transformedRay = ray;
    transformRay(transformedRay, vi.instance->rcp_xfm);

    // Set volume context initial values
    VolumeContext &vc = volumeContexts[i];
    vc.org = transformedRay.org;
    vc.dir = transformedRay.dir;
  }

  // Define initial color and transmission
  vec3f color = make_vec3f(0.f);
  float transmission = 1.f;

  // Iterate through all ray intervals
  for (uniform int i = 0;
       i < reduce_max(rayIntervals.count) && (transmission > 0.f);
       i++) {
    if (i >= rayIntervals.count)
      break;

    // Iterate through all volumes
    for (uniform int j = 0; j < reduce_max(volumeIntervals.numVolumeIntervals);
         j++) {
      if (j >= volumeIntervals.numVolumeIntervals)
        break;

      // Calculate volume and ray intervals union
      const VolumeInterval &vi = volumeIntervals.intervals[j];
      range1f rInterval = rayIntervals.intervals[i];
      rInterval.lower = max(rInterval.lower, vi.interval.lower);
      rInterval.upper = min(rInterval.upper, vi.interval.upper);

      // Reset distance to sample
      VolumeContext &vc = volumeContexts[j];
      vc.distance = inf;

      // Check if volume and ray intervals overlap
      if (rInterval.upper <= rInterval.lower) {
        // Skip this volume if not
        vc.ready = 1;
        continue;
      }

      // We will sample across this volume so initialize members used during
      // sampling
      vc.iuDistance = jitter;
      vc.iuLength = 0.f;
      vc.ready = 0;
      vc.interval.tRange.upper = inf;

      // There might be different volumetric models used across vector lanes
      // so we must iterate over them
      VolumetricModel *varying model = vi.volumetricModel;
      float time = 0.5f;
      foreach_unique (m in model) {
        // Create volume interval interator
        /*vc.intervalIterator = vklInitIntervalIteratorV(m->volume->vklSampler,
            (varying vkl_vec3f *)&vc.org,
            (varying vkl_vec3f *)&vc.dir,
            (varying vkl_range1f *)&rInterval,
            m->vklValueSelector,
            vc.intervalIteratorBuffer);*/

	    // Create volume interval interator
            vc.intervalIterator = vklInitIntervalIteratorV(m->vklIntervalContext,
            			(varying vkl_vec3f *)&vc.org,
            			(varying vkl_vec3f *)&vc.dir,
            			(varying vkl_range1f *)&rInterval,
            			&time,
            			vc.intervalIteratorBuffer);
      
      }
    }

    // Propagate ray across all volumes till opaque
    while (transmission > 0.f) {
      // Sample across all volumes
      vec4f sampledColor;
      float dist = sampleAllVolumes(rc,
          volumeIntervals,
          volumeContexts,
          ray,
          samplingRate,
          sampledColor,
          shade,
	  self
	  );

      // Exit loop if nothing sampled
      if (dist == inf)
        break;

      // Blend sampled color
      if (self->frontBackBlendMode == 0){
      	   color = color
            + transmission * (1.f - sampledColor.w) * make_vec3f(sampledColor); 
      	   //transmission *= sampledColor.w;
	   
	   // to alpha
      	   //float a_w = 1 - luminance(make_vec3f(transmission));
      	   //float b_w = 1 - luminance(make_vec3f(sampledColor.w));
      	   //struct vec4f a  = {color.x, color.y, color.z, a_w};
      	   //struct vec4f b = {sampledColor.x, sampledColor.y, sampledColor.z, b_w};
      	   //struct vec4f retCol = alphaBlend(a, b);
	   //b_w = (1-a_w)*b_w;
      	   //struct vec4f retCol = huePreserveBlend(a, b);

      	   // set back to transmission
      	   //color = make_vec3f(retCol);
	   
      	   transmission *= sampledColor.w;
      }else if (self->frontBackBlendMode == 1){
      	   // to alpha
      	   float a_w = 1 - luminance(make_vec3f(transmission));
      	   float b_w = 1 - luminance(make_vec3f(sampledColor.w));
      	   struct vec4f a  = {color.x, color.y, color.z, a_w};
      	   struct vec4f b = {sampledColor.x, sampledColor.y, sampledColor.z, b_w};
      	   //struct vec4f retCol = alphaBlend(a, b);
	   //b_w = (1-a_w)*b_w;
      	   struct vec4f retCol = huePreserveBlend(a, b);

      	   // set back to transmission
      	   color = make_vec3f(retCol);
	   
      	   transmission *= sampledColor.w;
      }else if (self->frontBackBlendMode == 2){
      	   float a_w = 1 - luminance(make_vec3f(transmission));
      	   float b_w = 1 - luminance(make_vec3f(sampledColor.w));
	   if (b_w > a_w) {
	      color = make_vec3f(sampledColor);
      	      transmission = sampledColor.w;
	   }
      }


      // Stop if we reached min contribution
      if (transmission < rc.renderer->super.minContribution)
        transmission = 0.f;
    }
  }

  // Return final color
  popTLS(volumeContexts);
  return make_vec4f(color, transmission);
}
