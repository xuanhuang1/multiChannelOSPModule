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
  Ref<const DataT<float> > bbox;
  Ref<const DataT<int> > renderAttributes;
  Ref<const DataT<float> > renderAttributesWeights;
  Ref<const DataT<uint8_t> > histMaskTexture; 
  Ref<const DataT<TransferFunction *> > tfs;
  Ref<const DataT<TransferFunction *> > distFns;
  Ref<const DataT<int> > segColWithAlphaModifier;
  std::vector<void *> tfIEs;
  std::vector<void *> distFnIEs;
};

} // namespace ospray
