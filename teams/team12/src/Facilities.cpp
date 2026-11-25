#include "Facilities.h"

#include "Geometry.h"

namespace team12 {

void drawFacilities(const StadiumLayout& layout) {
    // Ground markings reserve the three separate building sites.
    drawPad(layout.playerFacilities, previewMarkingHeight, palette::facadeWhite);
    drawPad(layout.ticketOffice, previewMarkingHeight, palette::facadeWhite);
    drawPad(layout.fanShop, previewMarkingHeight, palette::facadeWhite);
}

}  // namespace team12
