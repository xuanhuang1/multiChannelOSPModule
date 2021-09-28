// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MultivariantMaterial.ih"

export void *uniform MultivariantMaterial_create(void *uniform cppE)
{
  MultivariantMaterial *uniform mat = uniform new uniform MultivariantMaterial;

  mat->super.cppEquivalent = cppE;

  return mat;
}

export void MultivariantMaterial_set(void *uniform _mat,
    const uniform float d,
    const uniform TextureParam &dMap,
    const uniform vec3f &Kd,
    const uniform TextureParam &KdMap,
    const uniform vec3f &Ks,
    const uniform TextureParam &KsMap,
    const uniform float &Ns,
    const uniform TextureParam &NsMap,
    const uniform vec3f &Tf)
{
  MultivariantMaterial *uniform mat = (MultivariantMaterial * uniform) _mat;

  /*mat->super.type = RENDERER_TYPE_SCIVIS;*/

  mat->d = d;
  mat->dMap = dMap;
  mat->Kd = Kd;
  mat->KdMap = KdMap;
  mat->Ks = Ks;
  mat->KsMap = KsMap;
  mat->Ns = Ns;
  mat->NsMap = NsMap;
  mat->Tf = Tf;
}
