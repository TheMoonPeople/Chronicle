#include "dataalloc.hpp"

namespace dc::game {

CDataAlloc2<1>::CDataAlloc2() : CDataAlloc2(-1) {}

CDataAlloc<1, 6000>::CDataAlloc() : used(0) {}

} // namespace dc::game
