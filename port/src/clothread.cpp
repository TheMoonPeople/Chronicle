// clothread.hpp names CCloth without declaring it.
class CCloth;

#include "clothread.hpp"

#include "rect.hpp"
#include "texture_port.hpp"

// A local-to-local GS transfer between two images the registry knows by base pointer. The direction
// only orders overlapping transfers within one buffer, which the renderer's copy does not need.
PC_OVERRIDE void MoveImageTest(sceVif1Packet *packet, int src_base, int src_width, int src_format, const CRect_i_ &rect,
                               int dst_base, int dst_width, int dst_format, int dst_x, int dst_y, int direction) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    PortMoveImage(static_cast<unsigned>(src_base), static_cast<unsigned>(src_format), rect.x, rect.y, rect.width,
                  rect.height, static_cast<unsigned>(dst_base), static_cast<unsigned>(dst_format), dst_x, dst_y);
}

// The title overlay's units each define their own CRect<int> (x, y, w, h) and call MoveImageTest
// through it. MWCC gave that the same symbol as CRect_i_; clang does not, so the overlay's calls
// land here.
template <class T>
class CRect;

void MoveImageTest(sceVif1Packet *packet, int src_base, int src_width, int src_format, const CRect<int> &rect,
                   int dst_base, int dst_width, int dst_format, int dst_x, int dst_y, int direction) {
    const int *fields = reinterpret_cast<const int *>(&rect);
    MoveImageTest(packet, src_base, src_width, src_format, CRect_i_(fields[0], fields[1], fields[2], fields[3]),
                  dst_base, dst_width, dst_format, dst_x, dst_y, direction);
}
