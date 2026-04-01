#pragma once

#include <algorithm>
#include <vector>

namespace render {

struct DepthTarget {
  int width{0};
  int height{0};
  std::vector<float> data;

  DepthTarget() = default;

  DepthTarget(int widthValue, int heightValue)
      : width(widthValue),
        height(heightValue),
        data(static_cast<size_t>(widthValue) * heightValue, 1.0f) {}

  void Resize(int widthValue, int heightValue) {
    width = widthValue;
    height = heightValue;
    data.assign(static_cast<size_t>(width) * height, 1.0f);
  }

  void Clear(float clearValue) {
    std::fill(data.begin(), data.end(), clearValue);
  }
};

}  // namespace render
