#pragma once
#include <cstddef>
namespace dps {
// Missing samples replaced with zeros to preserve the recording's timeline.
struct AudioGap {std::size_t begin=0,frames=0;};
}
