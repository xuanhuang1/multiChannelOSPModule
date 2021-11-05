// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MultivariantMaterial.h"
// ispc
#include "multivariant/MultivariantMaterial_ispc.h"

namespace ospray {

MultivariantMaterial::MultivariantMaterial()
{
  ispcEquivalent = ispc::MultivariantMaterial_create(this);
}

std::string MultivariantMaterial::toString() const
{
  return "ospray::multivariant::obj";
}

void MultivariantMaterial::commit()
{
  MaterialParam1f d = getMaterialParam1f("d", 1.f);
  MaterialParam3f Kd = getMaterialParam3f("kd", vec3f(.8f));
  MaterialParam3f Ks = getMaterialParam3f("ks", vec3f(0.f));
  MaterialParam1f Ns = getMaterialParam1f("ns", 10.f);
  vec3f Tf = getParam<vec3f>("tf", vec3f(0.f));

  ispc::MultivariantMaterial_set(getIE(),
      d.factor,
      d.tex,
      (const ispc::vec3f &)Kd.factor,
      Kd.tex,
      (const ispc::vec3f &)Ks.factor,
      Ks.tex,
      Ns.factor,
      Ns.tex,
      (const ispc::vec3f &)Tf);
}

} // namespace ospray
