// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "render/Material.h"

namespace ospray {

struct MultivariantMaterial : public ospray::Material
{
  MultivariantMaterial();
  std::string toString() const override;
  void commit() override;
};

} // namespace ospray
