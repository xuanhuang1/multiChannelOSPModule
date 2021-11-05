// Copyright 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// ospray
#include "render/Renderer.h"
#include "volume/transferFunction/TransferFunction.h"

namespace ospray {

struct Multivariant : public Renderer
{
  Multivariant();
  std::string toString() const override;
  void commit() override;
  void *beginFrame(FrameBuffer *, World *) override;

 private:
  bool visibleLights{false};
  bool scannedVisibleLightList{true};
  Ref<const DataT<int> > renderAttributes;
  Ref<const DataT<float> > renderAttributesWeights;
  Ref<const DataT<TransferFunction *> > tfs;
  std::vector<void *> tfIEs;
};

} // namespace ospray